/*****************************************************************************************
*【プログラム】ポケット ラズディエーション カウンター
*
*【種別】放射線量測計測ソフトウエア
* 
*【バージョン】1.00
*
*【開発目的】
*　Raspberry Piに接続されたポケガからの信号を受け取り集計結果をコンソールまたはファイルへ
*　出力することで放射線量を可視化します。
*
*【製作者ホームページ】
*  http://www.orsx.net/blog/
*
*【動作環境】
*　マザーボード: Raspberry Pi
*　ガイガーカウンター: 組込版ポケットガイガーType5
*　オペレーティングシステム: Raspbian “wheezy”
*
*【注意事項】
*　組込版ポケットガイガーType5とRaspberry Piを接続するときは10kオームから22kオームで
*　プルアップする必要があります。詳しくはポケットガイガー公式サイトを御覧ください。
*
*【依存関係】
*　ライブラリに"wiringPi.h"を使用します。コンパイル前にインストールしておいてください。
*　入手先: https://projects.drogon.net/raspberry-pi/wiringpi/
*
*【コンパイル方法】
*　$ gcc rasdiation.c -o rasdiation -pthread -lwiringPi -lm
*
*【ライセンス情報】
*
*  CFIVE
*  
*  Copyright (C) 2013 The Information Technology Center, The ORBIT SPACE.
*  All rights reserved.
* 
*  This program is free software; you can redistribute it and/or modify it under
*  the terms of the GNU General Public License as published by the Free Software
*  Foundation; either version 2 of the License, or (at your option) any later
*  version.
* 
*  This program is distributed in the hope that it will be useful, but WITHOUT
*  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
*  FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
*  details.
* 
*  You should have received a copy of the GNU General Public License along with
*  this program; if not, write to the Free Software Foundation, Inc., 59 Temple
*  Place, Suite 330, Boston, MA 02111-1307 USA
*
*****************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <wiringPi.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <string.h>
#include <time.h>
#include <getopt.h>

/* ------------ここから関数数定義------------ */
// 割り込みハンドラ
void signal_interrupt(void);
void noise_interrupt(void);

// スレッド関数
void *gpio_interrupt(void *arg);
void *data_calculation(void *arg);
void *counter_sec_incr(void *arg);
void *counter_info_str(void *arg);
void *array_index_move(void *arg);
/* ------------ ここまで関数数定義 ------------ */

/* -------- ここからグローバル変数定義 -------- */
// スレッドの排他制御
pthread_mutex_t mutex;

// 係数
const double alpha = 53.032;

// ガイガーカウンターからの信号の回数
int signCount = 0;
int noisCount = 0;

// CPM計算
int cpmIndex = 0;
double cpmCount = 0.0;
double cpmHistory[200] = {0.0};

// シグナルピン
int signPin = 0;
// ノイズピン
int noisPin = 0;
// ファイルのパス
char *outPut = NULL;
// 標準出力
int existence = 0;
// デバッグモード
int debugMode = 0;
// ビープ音
int signBeep = 0;
/* -------- ここまでグローバル変数定義 -------- */

int main(int argc, char* argv[]){

    // コマンドラインオプション
    char *opt_str = "s:n:o:embhv";
    struct option long_opts[] = {
        {"sign_pin",  required_argument, 0, 's'},
        {"nois_pin",  required_argument, 0, 'n'},
        {"output",    required_argument, 0, 'o'},
        {"existence", no_argument,       0, 'e'},
        {"mode",      no_argument,       0, 'm'},
        {"beep",      no_argument,       0, 'b'},
        {"help",      no_argument,       0, 'h'},
        {"version",   no_argument,       0, 'v'},
        {0,           0,                 0,   0}
    };

    int result = 0;
    int opt_idx = 0;

    char *buff;

    while ((result = getopt_long(argc, argv, opt_str, long_opts, &opt_idx)) != -1) {
        switch (result) {
            case 's':
                signPin = (int)strtol(optarg, &buff,10);
                if(*buff != '\0'){
                    fprintf(stderr, "Error: The value of the --sign_pin must be a number of GPIO.\n");
                    exit(1);
                }
                break;
            case 'n':
                noisPin = (int)strtol(optarg, &buff,10);
                if(*buff != '\0'){
                    fprintf(stderr, "Error: The value of the --nois_pin must be a number of GPIO.\n");
                    exit(1);
                }
                break;
            case 'o':
                outPut = optarg;
                break;
            case 'e':
                existence = 1;
                break;
            case 'm':
                debugMode = 1;
                break;
            case 'b':
                signBeep = 1;
                break;
            case 'h':
                printf("Usage:    This program is an example.\n");
                printf("$ rasdiation --sign_pin 30 --nois_pin 31 --mode --beep\n");
                printf("\n");
                printf("Option:   Option list of the available\n");
                printf("      --sign_pin  -s: [int]    Signal GPIO number.\n");
                printf("      --nois_pin  -n: [int]    Noise GPIO number.\n");
                printf("      --output    -o: [string] File path to write out.\n");
                printf("      --existence -e: [none]   Forced to console display.\n");
                printf("      --mode      -m: [none]   Debug mode.\n");
                printf("      --beep      -b: [none]   Beep sound.\n");
                printf("\n");
                printf("Hardwear: This software can be performed on the following hardware. \n");
                printf("Raspberry Pi:       http://www.raspberrypi.org/\n");
                printf("PocketGeiger Type5: http://www.radiation-watch.org/\n");
                printf("Signal pin and Noise pin, please pull up between 10k ohm 22k ohm from absolutely.\n");
                exit(0);
            case 'v':
                printf("Pocket Rasdiation Counter Version 1.00\n");
                printf("Copyright (c) 2013 Free Software Foundation, Inc.\n");
                printf("License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n");
                printf("This is free software: you are free to change and redistribute it.\n");
                printf("There is NO WARRANTY, to the extent permitted by law.\n");
                printf("\n");
                printf("Written by ORBIT SPACE\n");
                exit(0);
            default:
                fprintf(stderr, "Error: An unknown option is appointed.\n");
                exit(1);
        }
    }

    if(signPin <= 0 || noisPin <=0){
        fprintf(stderr, "Error: The value of the --sign_pin --nois_pin This must always be set.\n");
        exit(1);
    }

    pthread_t thread_gpio;
    pthread_t thread_data;
    pthread_t thread_info;
    pthread_t thread_move;

    pthread_mutex_init(&mutex, NULL);

    pthread_create(&thread_gpio, NULL, gpio_interrupt, (void *) NULL);
    pthread_create(&thread_data, NULL, data_calculation, (void *) NULL);
    pthread_create(&thread_info, NULL, counter_info_str, (void *) NULL);
    pthread_create(&thread_move, NULL, array_index_move, (void *) NULL);

    pthread_join(thread_gpio, NULL);
    pthread_join(thread_data, NULL);
    pthread_join(thread_info, NULL); 
    pthread_join(thread_move, NULL);

    pthread_mutex_destroy(&mutex);

    return 0;
}

/* -------- ここから割り込みハンドラ -------- */
// シグナルカウンタをインクリメントする
void signal_interrupt(void){
     pthread_mutex_init(&mutex, NULL);
     signCount++;
     if(signBeep > 0) printf("Signal\n");
     if(signBeep > 0) printf("\a");
     pthread_mutex_destroy(&mutex);
}

// ノイズカウンタをインクリメントする
void noise_interrupt(void){
     pthread_mutex_init(&mutex, NULL);
     noisCount++;
     if(signBeep > 0) printf("Noise\n");
     if(signBeep > 0) printf("\a");
     pthread_mutex_destroy(&mutex);
}
/* -------- ここまで割り込みハンドラ -------- */

/* ---------- ここからスレッド関数 ---------- */
// 割り込みを監視する
void *gpio_interrupt(void *arg){
     while(wiringPiSetupSys() != -1){
         wiringPiISR( signPin, INT_EDGE_FALLING, signal_interrupt );
         wiringPiISR( noisPin, INT_EDGE_RISING, noise_interrupt );
         sleep(10000);
     }
}

// 200ミリ秒ごとに取得したデータを集計する
void *data_calculation(void *arg){
    while(1){
        usleep(200000);

        pthread_mutex_lock(&mutex);

        if(noisCount <= 0){
            cpmHistory[cpmIndex] += signCount;
            cpmCount += signCount;
        }

        if(debugMode > 0) printf("data_calculation %lf\n", cpmCount);

        // カウンタ初期化       
        signCount=0;
        noisCount=0;

        pthread_mutex_unlock(&mutex);
    }
}

// 1秒ごとに出力する
void *counter_info_str(void *arg){
    int secCount = 0;
    double min = 0.0;
    double cpm = 0.0;
    double usvh = 0.0;
    double usvht = 0.0;

    int fd = 0;
    char str[255] = "";

    while(1){
        usleep(1000000);

        pthread_mutex_lock(&mutex);

        // 20分たつまでインクリメント
        if(secCount < (20 * 60)){
            secCount++;
        }

        // 各種線量の単位へ計算
        if(secCount >= 0){
            min = (double)secCount / 60.0;
            cpm = cpmCount / min;
            usvh = cpm / alpha;
            usvht = sqrt(cpmCount) / min / alpha;
        } else {
            cpm  = 0.0;
            usvh = 0.0;
            usvht = 0.0;
        }

        // 集計データの整形と出力
        sprintf(
            str,
            "UnixTime,%ld,CPM,%lf,uSv/h,%lf,uSv/h_Error,%lf\n",
            time(NULL), cpm, usvh, usvht
        );

        // 指定されたパスへ出力し、指定がなければ標準出力
        if(outPut != NULL){
            fd = open(outPut, O_WRONLY | O_NONBLOCK | O_APPEND | O_CREAT, 0644);
            if(fd != -1){
                if (flock(fd, LOCK_EX) == 0) {
                    write(fd, str, strlen(str));
                    flock(fd, LOCK_UN);
                } else {
                    perror("flock");
                }
            }

            if(existence > 0) printf("%s", str);
            close(fd);
        } else {
            printf("%s", str);
        } 

        pthread_mutex_unlock(&mutex);
    }

}

// 6秒ごとに配列をずらす
void *array_index_move(void *arg){
    while(1){
        usleep(6000000);

        pthread_mutex_lock(&mutex);

        // 配列のインデックスをインクリメントする
        cpmIndex++;

        // 配列が一周した場合インデックスを0に初期化する
        if(cpmIndex >= 200){
            cpmIndex = 0;
        }

        // 次に格納する配列に値が入っていれば引いて0に初期化する
        if(cpmHistory[cpmIndex] > 0){
            cpmCount -= cpmHistory[cpmIndex];
            cpmHistory[cpmIndex] = 0;
        }

        if(debugMode > 0) printf("array_index_move %d\n", cpmIndex);

        pthread_mutex_unlock(&mutex);
    }
}
/* ---------- ここまでスレッド関数 ---------- */

