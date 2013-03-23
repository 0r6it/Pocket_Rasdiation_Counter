#!/usr/bin/env python
# -*- coding: utf-8 -*-
import sys
import tweepy
import time
import locale

# Twitter 認証情報
CONSUMER_KEY    = '*******************************************'
CONSUMER_SECRET = '*******************************************'
ACCESS_KEY      = '*******************************************'
ACCESS_SECRET   = '*******************************************'

# Rasdiationの出力する名前付きパイプまでのパス
FIFO_PATH       = '/var/lib/rasdiation/rasdiation.fifo'

# CSVを分解して配列に入れる関数
def get_geiger_datas():
    file = open(FIFO_PATH)
    line = file.readline()
    file.close
    geiger_datas = line.split(',')
    return geiger_datas

# Tweet内容の生成する関数
def make_post_content(datas):
    # 日本語での日付表示の為にローケルの指定
    locale.setlocale(locale.LC_ALL, 'ja_JP.utf8')
    utime    = time.localtime(float(datas[1]))
    datetime = time.strftime("%Y年%m月%d日(%a) %H時%M分", utime)
    # 各種計測値を変数に格納
    cpm      = float(datas[3])
    usvh     = float(datas[5])
    usvhe    = float(datas[7])
    # Tweet内容のフォーマットを定義
    format   = "%s 現在の放射線量は CPM:%2.3f uSv/h:%2.5f(誤差 %2.5f) でした。"
    # Tweet内容の作成
    data     = format % (datetime, cpm, usvh, usvhe)
    return data

# Twitterへ認証・投稿する関数
def post_content(mypost):
    auth = tweepy.OAuthHandler(CONSUMER_KEY, CONSUMER_SECRET)
    auth.set_access_token(ACCESS_KEY, ACCESS_SECRET)
    api = tweepy.API(auth)
    api.update_status(mypost)
    return

# メイン関数
if __name__ == '__main__':
    try:
        datas   = get_geiger_datas()
        content = make_post_content(datas)
        post_content(content)
    except Exception:
        print >>sys.stderr, "Tweet Error"
