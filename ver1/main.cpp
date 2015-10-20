//
//  main.cpp
//  ver1
//
//  Created by 喜楽 智規 on 2015/06/16.
//  Copyright (c) 2015年 Kiraku Tomoki. All rights reserved.
//

//マウスでウィンドウの領域を指定して，指定された領域のみをキャプチャする
//その後牌領域のみ抽出する．
/*
 実行方法
 Webカメラを接続し，本プロジェクトをRun．
 最初にキャプチャ画像が表示されるが，PCにカメラが内蔵だとそちらのカメラキャプチャ画像が表示されるかも
 その場合は
 #define CAM_NUM　0を1，1を0に直すと上手くいく．
 
 「Capture」windowにキャプチャ画像が表示された後，キャプチャ画像のウィンドウでキャプチャした領域の
 1.左上
 2.右下
 の順にクリックし，Escキーを押すと，選択した領域のみのキャプチャが行える．
 
 その後「Capture2」というwindowが表示されるので，任意の牌を1つ選び，牌の左上，右下をクリックする．これにより牌の大きさを認識する．
 クリック後はEscキーを押す．
 
 牌認識後は「スペースキー」を押すとキャプチャできる(学習データ用)
 
 
 wekaについて
 pai.model: 牌34種を学習させたファイル．アルゴリズムはランダムフォレスト
 testdata.arff: クラスが未知のレコードの特徴量を書きだしたファイル
 output.txt: クラスが未知のレコードに対し予想した結果を書き出し
 */




//使用WEBカメラ　logicool C920  http://www.logicool.co.jp/ja-jp/product/hd-pro-webcam-c920

#define CAM_NUM 1 //大体0か1を指定
//#define SRC_WIDTH 1920
//#define SRC_HEIGHT 1080
#define SRC_WIDTH 960
#define SRC_HEIGHT 540
#define OUTPUT_SIZE 3
#define PAI_WIDTH 27*5
#define PAI_HEIGHT 20*5
//特徴量抽出時の分割数を定義．縦の分割数:横の分割数≒20:27になるような数値で分割してみる．
//縦の分割数
#define TATE_DIV 5
//横の分割数
#define YOKO_DIV 7

#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <ctype.h>
#include "./Labeling.h"
#include <time.h>
#include <vector>
#include <iostream>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opencv/cv.h"
#include "opencv/highgui.h"

//プロトタイプ宣言
int sum(int *data,int num);
double average(int *data,int num);
double variance(int *data,double average,int num);
void on_mouse (int event, int x, int y, int flags, void *param);
void on_mouse2 (int event, int x, int y, int flags, void *param);
void calcFeaturevalue(IplImage* src_img, double* output);

//ソート用関数
bool compareFuncX(const CvScalar& left, const CvScalar& right)
{
    //降順ソート:left>right    昇順ソート: left<right
    return left.val[0] > right.val[0];
}

bool compareFuncY(const CvScalar& left, const CvScalar& right)
{
    //降順ソート:left>right    昇順ソート: left<right
    return left.val[1] > right.val[1] ;
}

int clickX[2]={100,100},clickY[2]={300,300};
int inx=0;
int clickXPai[2]={100,100},clickYPai[2]={300,300};

int upper_pai_left=-5;
int upper_pai_right=5;
int keymode=0;

int main (int argc, char **argv)
{
    CvCapture *capture = 0;
    IplImage *frame = 0;
    IplImage *roiSetFrame = 0;
    IplImage *roiSet4Affine=0;
    IplImage *affineImage=0;
    IplImage *paiImageAffine=0;
    IplImage *paiImageAffine2=0;
    IplImage *paiImageAffineOtsu=0;
    IplImage *paiImageAffineGray=0;
    IplImage *saveImage=0;
    
    int c;
    int paiNum=13;  //画像中の牌の数を手動で指定．
    double heightPai = 0.0;
    
    //キャプチャ構造体を作成する
    capture = cvCreateCameraCapture (CAM_NUM);
    
    //キャプチャサイズを設定
    cvSetCaptureProperty (capture, CV_CAP_PROP_FRAME_WIDTH, SRC_WIDTH);
    cvSetCaptureProperty (capture, CV_CAP_PROP_FRAME_HEIGHT, SRC_HEIGHT);
    
    //キャプチャ画像の表示ウィンドウの作成とマウスイベントコールバック関数の指定
    cvNamedWindow ("Capture", CV_WINDOW_AUTOSIZE);
    cvSetMouseCallback("Capture", on_mouse);
    
    while(1){
        //まず，ROIを設定するためにカメラから画像をキャプチャし，その画像を表示する
        frame = cvQueryFrame (capture);
        for(int i=1;i<=10;i++){
            cvLine(frame, cvPoint(0, frame->height*i/10), cvPoint(frame->width, frame->height*i/10), cvScalar(0,0,0));
        }
        cvShowImage("Capture", frame);
        
        
        //ROIの設定(左上クリック，右下クリック)を行った後にユーザがescキーを押す
        c = cvWaitKey (10);
        if (c == '\x1b'){   //escキー押したら
            cvDestroyWindow("Capture");
            break;
        }
    }
    
    //牌縦横比算出時のマウスイベントコールバック関数の指定
    cvNamedWindow ("Capture2", CV_WINDOW_AUTOSIZE);
    cvSetMouseCallback("Capture2",on_mouse2);
    //次に牌の縦横比を算出するために牌(1つ分)の左上，右下をクリックする．
    inx=0;
    frame = cvQueryFrame (capture);
    cvShowImage("Capture2", frame);
    while(1){
        c = cvWaitKey (10);
        if (c == '\x1b'){   //escキー押したら
            cvDestroyWindow("Capture2");
            break;
        }
    }
    
    //Capture2でクリックした値より，牌の縦横比を求める
    double paiRatio=(clickYPai[1]-clickYPai[0])/(double)(clickXPai[1]-clickXPai[0]);
    
    //setしたROI部分のみのキャプチャを撮り続ける
    //cvNamedWindow("roiSetImage");
    
    while(1){
        //キャプチャし，ROIを指定
        frame=cvQueryFrame(capture);
        cvSetImageROI(frame, cvRect(clickX[0], clickY[0], clickX[1]-clickX[0], clickY[1]-clickY[0]));
        //ROI指定後の画像をコピー
        roiSetFrame=cvCloneImage(frame);
        //ROIの解除
        cvResetImageROI(frame);
        
        roiSet4Affine=cvCloneImage(frame);
        //cvShowImage("debug1", frame);
        
        //ROI指定後の画像をグレースケール化
        IplImage *grayROI=cvCreateImage(cvGetSize(roiSetFrame), IPL_DEPTH_8U, 1);
        cvCvtColor (roiSetFrame, grayROI, CV_BGR2GRAY);
        
        //ラプラスフィルタによるエッジ検出
        IplImage *edgeROI=cvCreateImage(cvGetSize(roiSetFrame), IPL_DEPTH_8U, 1);
        
        //cvSobel(grayROI, edgeROI, 0, 1, 3);
        cvLaplace(grayROI, edgeROI,3);
        
        //4debug
        //cvShowImage("edgeROI!!!", edgeROI);
        
        
        //ラベリング処理
        //ここはlabering.hのバグ？により別の方法で牌の下部を探索することにする
        
        //手動で牌下部の探索．
        //牌下部の座標を保持するvector型コンテナ
        std::vector<CvScalar> coordinate;
        std::vector<CvScalar> coordinate_head;
        std::vector<CvScalar> coordinate_tail;
        
        //edgeROIの画像の中でwidth/2をheightの大きい順に探索し，エッジ領域を探す．
        for(int j=0;j<edgeROI->width;j+=4){
            for(int i=edgeROI->height-1;i>edgeROI->height/2;i--){
                //画素にアクセス
                if(cvGet2D(edgeROI, i, j).val[0]>50){
                    cvCircle(roiSetFrame, cvPoint(j, i), 1, CV_RGB(0,255,0),-1);
                    //vectorに座標を保持
                    coordinate.push_back(cvScalar(j,i));
                    break;
                }
            }
        }
        
        //先頭要素15個抜き出し
        //牌座標下部の直線を引くときの始点と終点
        CvPoint start,end;
        
        if(!coordinate.empty()){
            
            if(coordinate.size()>=15){
                for(int i=0;i<15;i++){
                    //先頭15要素をcoordinate_headに入れる．
                    coordinate_head.push_back(coordinate[i]);
                }
            }
            //yをキーにしてソートする
            std::sort(coordinate_head.begin(),coordinate_head.end(),compareFuncY);
            int start_y=0;
            if(coordinate_head.size()>=10){
                for(int i=0;i<10;i++){
                    start_y+=coordinate_head[i].val[1];
                }
            }
            start_y=start_y/10.0;
            
            //xをキーにしてソートする
            std::sort(coordinate_head.begin(),coordinate_head.end(),compareFuncX);
            if(coordinate_head.size()>0){
                start=cvPoint(coordinate_head[coordinate_head.size()-1].val[0], start_y);
            }
            //同様に末端要素15要素の抜き出し
            //coordinate.size()はsize_t型→unsigned long
            if(coordinate.size()>=15){
                for(int i=(int)coordinate.size()-1;coordinate.size()-16<i;i--){
                    //末尾15要素をcoordinate_tailに入れる．
                    coordinate_tail.push_back(coordinate[i]);
                }
            }
            
            //yをキーにしてソートする
            std::sort(coordinate_tail.begin(),coordinate_tail.end(),compareFuncY);
            
            //y座標の和より最大と最小を取り除き(外れ値の取り除き)平均を求める．
            //牌座標下部の直線を引くときの始点と終点
            
            int end_y=0;
            if(coordinate_tail.size()>=10){
                for(int i=0;i<10;i++){
                    end_y+=coordinate_tail[i].val[1];
                }
            }
            end_y=end_y/10.0;
            //xをキーにしてソートする
            std::sort(coordinate_tail.begin(),coordinate_tail.end(),compareFuncX);
            if(coordinate_tail.size()>0){
                end=cvPoint(coordinate_tail[0].val[0], end_y);
            }
            printf("endx:%d,endy:%d\n",end.x,end.y);
            
            //牌の下部の直線の描画
            cvLine(roiSetFrame, start, end, cvScalar(255,0,0));
            
        }
        
        
        //牌の縦横比を使って牌領域を線で囲む
        //牌下の線の長さ
        double lengthPai=sqrt(pow(end.x-start.x,2)+pow(end.y-start.y, 2));
        //求めた縦横比を使って高さを算出
        if(lengthPai*paiRatio/(double)paiNum<1280){
            heightPai=lengthPai*paiRatio/(double)paiNum;
        }
        //printf("heightPai:%f\n",heightPai);
        //printf("start.x:%d, start.y-heightPai:%f\n",start.x, start.y-heightPai);
        //牌上部の直線を引く
        cvLine(roiSetFrame, cvPoint(start.x+upper_pai_left, start.y-heightPai), cvPoint(end.x+upper_pai_right,end.y-heightPai), cvScalar(255,0,0));
        
        //牌上部・下部の線に囲まれた領域のみを抽出し，paiNum分だけ分割する→牌1つ1つの画像に分割する．
        //左上座標 cvPoint(start.x, start.y-heightPai)
        //右下座標 end
        //まず現在のROIを解除
        int xOffset,yOffset;
        xOffset=roiSetFrame->roi->xOffset;
        yOffset=roiSetFrame->roi->yOffset;
        cvResetImageROI(roiSetFrame);
        //その後新しいROIをセット
        //ROIの指定がマイナスなど不正な値でないかをチェックしてからROIを指定する．
        if(heightPai<100 && start.x>0 && start.y>0 && end.x>0 && end.y>0){
            //cvSetImageROI(roiSetFrame, cvRect(start.x+xOffset, start.y-(int)heightPai+yOffset, end.x-start.x, end.y-start.y+(int)heightPai));
        }
        
        //アフィン変換し，歪みがない・牌部分のみの画像にする．
        //画像上の4点対応により透視投影変換行列を計算し，その行列を用いて画像全体の透視投影変換を行う．
        //http://opencv.jp/sample/sampling_and_geometricaltransforms.html
        
        CvPoint2D32f src_point[4],dst_point[4];
        CvMat *map_matrix;  //アフィン変換用変換行列
        
        //牌上部の直線を引く
        //cvLine(roiSetFrame, cvPoint(start.x, start.y-heightPai), cvPoint(end.x,end.y-heightPai), cvScalar(255,0,0));
        //牌の下部の直線の描画
        //cvLine(roiSetFrame, start, end, cvScalar(255,0,0));
        
        //左上から時計回りの順
        src_point[0]=cvPoint2D32f(start.x+xOffset+upper_pai_left, start.y-heightPai+yOffset);
        src_point[1]=cvPoint2D32f(end.x+xOffset+upper_pai_right,end.y-heightPai+yOffset);
        src_point[2]=cvPoint2D32f(end.x+xOffset, end.y+yOffset);
        src_point[3]=cvPoint2D32f(start.x+xOffset, start.y+yOffset);
        
        dst_point[0]=cvPoint2D32f(0, 0);
        dst_point[1]=cvPoint2D32f(20*paiNum*OUTPUT_SIZE, 0);
        dst_point[2]=cvPoint2D32f(20*paiNum*OUTPUT_SIZE, 27*OUTPUT_SIZE);
        dst_point[3]=cvPoint2D32f(0,27*OUTPUT_SIZE);
        
        map_matrix=cvCreateMat(3, 3, CV_32FC1);
        cvGetPerspectiveTransform(src_point, dst_point, map_matrix);
        //cvShowImage("debug1", roiSet4Affine);
        //cvShowImage("debug2", roiSetFrame);
        
        //変換後画像
        paiImageAffine=cvCreateImage(cvSize(20*paiNum*OUTPUT_SIZE, 27*OUTPUT_SIZE), IPL_DEPTH_8U, 3);
        //指定された透視投影変換行列により，cvWarpPerspectiveを用いて画像を変換させる
        cvWarpPerspective (roiSet4Affine, paiImageAffine, map_matrix, CV_INTER_LINEAR + CV_WARP_FILL_OUTLIERS, cvScalarAll (100));
        
        //抜き出し画像の表示
        //cvShowImage("paiImageAffine", paiImageAffine);
        paiImageAffineGray=cvCreateImage(cvSize(paiImageAffine->width, paiImageAffine->height), IPL_DEPTH_8U, 1);
        cvCvtColor (paiImageAffine,paiImageAffineGray, CV_BGR2GRAY);
        paiImageAffineOtsu=cvCreateImage(cvSize(paiImageAffine->width, paiImageAffine->height), IPL_DEPTH_8U, 1);
        cvThreshold (paiImageAffineGray, paiImageAffineOtsu, 0, 255, CV_THRESH_BINARY | CV_THRESH_OTSU);
        //cvShowImage("paiImageAffineOtsu", paiImageAffineOtsu);
        //抜き出し画像の内，左右の余白を消去する．
        //左上
        for(int i=0;i<paiImageAffine->width/2;i++){
            if (cvGet2D(paiImageAffineOtsu, 0, i).val[0]>128){
                src_point[0]=cvPoint2D32f(i, 0);
                break;
            }
        }
        
        
        //左下
        for(int i=0;i<paiImageAffine->width/2;i++){
            if (cvGet2D(paiImageAffineOtsu,paiImageAffine->height-7,i).val[0]>128){
                src_point[3]=cvPoint2D32f(i, paiImageAffine->height-7);
                break;
            }
        }
        
        //右上
        for(int i=paiImageAffine->width-1;i>paiImageAffine->width/2;i--){
            if (cvGet2D(paiImageAffineOtsu, 0, i).val[0]>128){
                src_point[1]=cvPoint2D32f(i, 0);
                break;
            }
        }
        
        
        //右下
        for(int i=paiImageAffine->width-1;i>paiImageAffine->width/2;i--){
            if (cvGet2D(paiImageAffineOtsu,paiImageAffine->height-7, i).val[0]>128){
                src_point[2]=cvPoint2D32f(i, paiImageAffine->height-7);
                break;
            }
        }
        
        
        dst_point[0]=cvPoint2D32f(0, 0);
        dst_point[1]=cvPoint2D32f(20*paiNum*OUTPUT_SIZE, 0);
        dst_point[2]=cvPoint2D32f(20*paiNum*OUTPUT_SIZE, 27*OUTPUT_SIZE);
        dst_point[3]=cvPoint2D32f(0,27*OUTPUT_SIZE);
        
        map_matrix=cvCreateMat(3, 3, CV_32FC1);
        cvGetPerspectiveTransform(src_point, dst_point, map_matrix);
        //cvShowImage("debug1", roiSet4Affine);
        //cvShowImage("debug2", roiSetFrame);
        
        //変換後画像
        paiImageAffine2=cvCreateImage(cvSize(20*paiNum*OUTPUT_SIZE, 27*OUTPUT_SIZE), IPL_DEPTH_8U, 3);
        IplImage *paiImageAffineWithLine=cvCreateImage(cvSize(20*paiNum*OUTPUT_SIZE, 27*OUTPUT_SIZE), IPL_DEPTH_8U, 3);
        //指定された透視投影変換行列により，cvWarpPerspectiveを用いて画像を変換させる
        cvWarpPerspective (paiImageAffine, paiImageAffine2, map_matrix, CV_INTER_LINEAR + CV_WARP_FILL_OUTLIERS, cvScalarAll (100));
        cvCopy(paiImageAffine2, paiImageAffineWithLine);
        //paiNum分割の線を引く
        for(int i=1;i<paiNum+1;i++){
            cvLine(paiImageAffineWithLine, cvPoint(paiImageAffine->width*i/paiNum, 0), cvPoint(paiImageAffine->width*i/paiNum, paiImageAffine->height), cvScalar(255,0,0));
        }
        
        //ROI指定後の画像をグレースケール化
        //        IplImage *grayROI2=cvCreateImage(cvGetSize(paiImageAffine2), IPL_DEPTH_8U, 1);
        //        cvCvtColor (paiImageAffine2, grayROI2, CV_BGR2GRAY);
        //        cvSobel(paiImageAffine2, paiImageAffine2, 1, 0);
        //
        cvShowImage("2", paiImageAffineWithLine);
        cvReleaseImage(&paiImageAffineWithLine);
        
        //----------ここから特徴量計算などの所----------
        //このプログラムで計算した特徴量を書き出すfp
        FILE *outputFP;
        //wekaでの分類結果を読み取るためのfp
        FILE *inputFP;
        //ファイルを読む時用のバッファ
        //char s[256];
        
        //fileOpen
        if ((outputFP = fopen("testdata.arff", "w")) == NULL) {
            printf("file open error!!\n");
            exit(EXIT_FAILURE);
        }
        //arffのヘッダ書き込み
        fprintf(outputFP, "@relation testdata\n@attribute YOKO0 numeric\n@attribute YOKO1 numeric\n@attribute YOKO2 numeric\n@attribute YOKO3 numeric\n@attribute YOKO4 numeric\n@attribute YOKO5 numeric\n@attribute YOKO6 numeric\n@attribute TATE4 numeric\n@attribute TATE3 numeric\n@attribute TATE2 numeric\n@attribute TATE1 numeric\n@attribute TATE0 numeric\n@attribute SUMTplusSUMY numeric\n@attribute aveY numeric\n@attribute aveT numeric\n@attribute variT numeric\n@attribute variY numeric\n@attribute class {p1,p2,p3,p4,p5,p6,p7,p8,p9,s1,s2,s3,s4,s5,s6,s7,s8,s9,m1,m2,m3,m4,m5,m6,m7,m8,m9,z1,z2,z3,z4,z5,z6,z7}\n\n@data\n");
        
        //paiNum分ループ回して特徴量計算とクラス予想する
        for(int i=0;i<paiNum;i++){
            IplImage *calc_img;
            calc_img=cvCreateImage(cvSize(paiImageAffine2->width/paiNum, paiImageAffine2->height), IPL_DEPTH_8U, 3);
            
            //画像の切り出し．paiNum分割する．
            cvSetImageROI(paiImageAffine2, cvRect(paiImageAffine2->width*i/paiNum, 0, paiImageAffine2->width/paiNum, paiImageAffine2->height));
            calc_img=cvCloneImage(paiImageAffine2);
            cvResetImageROI(paiImageAffine2);
            
            //特徴量計算
            double value[17];
            calcFeaturevalue(calc_img,value);
            
            //特徴量を書き出し
            for(int j=0;j<17;j++){
                fprintf(outputFP,"%f,",value[j]);
            }
            fprintf(outputFP,"?\n");
            cvReleaseImage(&calc_img);
        }
        
        //file close
        fclose(outputFP);
        
        //weka呼出し
        //クラスパスの指定 weka.jarへのパス
        //system("export CLASSPATH=/Applications/weka-3-6-11-oracle-jvm.app/Contents/Java/weka.jar:$CLASSPATH\n");
        system("java -classpath /Applications/weka-3-6-11-oracle-jvm.app/Contents/Java/weka.jar weka.classifiers.trees.RandomForest -T testdata.arff -l tree0616.model -p last > output.txt");
        
        //output.txtの読み出し
        if ((inputFP = fopen("output.txt", "r")) == NULL) {
            printf("file open error!!\n");
            exit(EXIT_FAILURE);
        }
        char tempChar;
        
        //EOFまで読み込み
        printf("--------------------\n");
        while((tempChar=fgetc(inputFP))!=EOF){
            if (tempChar==':') {
                if((tempChar=fgetc(inputFP))!='?'){
                    printf("%c",tempChar);
                    printf("%c",fgetc(inputFP));
                    printf("\n");
                }
            }
        }
        printf("--------------------\n");
        
        //コンテナ要素の削除
        coordinate.clear();
        coordinate_head.clear();
        coordinate_tail.clear();
        
        //表示
        //ROI指定画像の表示
        //cvShowImage("roiSetImage", roiSetFrame);
        //ROI指定画像(二値化後)の表示
        //cvShowImage("grayROI", grayROI);
        //エッジ検出画像の表示
        //cvShowImage("egde", edgeROI);
        //ROI画像+ラベリング結果
        //cvShowImage("Labering", roiSetFrame);
        
        
        //キー入力で調整する
        
        //keymode:0　paiRatioの設定
        //keymode:1　牌上部の左側
        //keymode:2 牌上部の右側
        
        c = cvWaitKey (300);
        if (c == '\x1b'){   //escキー押したら
            
        }
        else if(c == 'm'){
            if(keymode==0){
                paiRatio-=0.01;
            }
            else if(keymode==1){
                upper_pai_left--;
            }
            else if(keymode==2){
                upper_pai_right--;
            }
        }
        else if(c== 'p'){
            if(keymode==0){
                paiRatio+=0.01;
            }
            else if(keymode==1){
                upper_pai_left++;
            }
            else if(keymode==2){
                upper_pai_right++;
            }
        }
        //mode
        else if(c=='0'){
            printf("keymode:0\n");
            keymode=0;
        }
        else if(c=='1'){
            printf("keymode:1\n");
            keymode=1;
        }
        else if(c=='2'){
            printf("keymode:2\n");
            keymode=2;
        }
        //capture
        else if(c==32){
            //space key
            //牌キャプチャする
            saveImage=cvCreateImage(cvSize(paiImageAffine2->width/paiNum, paiImageAffine2->height), IPL_DEPTH_8U, 3);
            
            char savepath[30];
            time_t timer;
            struct tm *t_st;
            
            /* 現在時刻の取得 */
            time(&timer);
            t_st = localtime(&timer);
            
            //画像を分割して，1枚ずつ保存する
            for(int i=0;i<paiNum;i++){
                //ファイル名
                sprintf(savepath, "%d-%d-%d-%d-%d-%d.jpg",i,t_st->tm_mon+1,t_st->tm_mday,t_st->tm_hour,t_st->tm_min,t_st->tm_sec);
                
                cvSetImageROI(paiImageAffine2, cvRect(paiImageAffine2->width*i/paiNum, 0, paiImageAffine2->width/paiNum, paiImageAffine2->height));
                saveImage=cvCloneImage(paiImageAffine2);
                cvSaveImage(savepath, saveImage);
                cvResetImageROI(paiImageAffine2);
                cvReleaseImage(&saveImage);
                
            }
        }
        
        //解放
        cvReleaseImage(&roiSetFrame);
        cvReleaseImage(&grayROI);
        cvReleaseImage(&edgeROI);
        cvReleaseImage(&affineImage);
        cvReleaseImage(&paiImageAffine);
        cvReleaseImage(&roiSet4Affine);
        cvReleaseImage(&paiImageAffine2);
        cvReleaseImage(&paiImageAffineOtsu);
        cvReleaseImage(&paiImageAffineGray);
        
    }//while終
    return 0;
}

void on_mouse(int event, int x, int y, int flags, void *param=NULL){
    if (event == CV_EVENT_LBUTTONDOWN && inx<2) {
        printf("clicked\n");
        clickX[inx]=x+x%4;
        clickY[inx]=y+y%4;
        inx++;
    }
    else if(event == CV_EVENT_LBUTTONDOWN && inx>=2){
        printf("ONE:x:%d,y:%d\n",clickX[0],clickY[0]);
        printf("TWO:x:%d,y:%d\n",clickX[1],clickY[1]);
    }
    
}

void on_mouse2(int event, int x, int y, int flags, void *param=NULL){
    if (event == CV_EVENT_LBUTTONDOWN && inx<2) {
        printf("clicked\n");
        clickXPai[inx]=x;
        clickYPai[inx]=y;
        inx++;
    }
    else if(event == CV_EVENT_LBUTTONDOWN && inx>=2){
        printf("ONE:x:%d,y:%d\n",clickXPai[0],clickYPai[0]);
        printf("TWO:x:%d,y:%d\n",clickXPai[1],clickYPai[1]);
    }
    
}
int sum(int *data, int num){
    int result=0.0;
    for(int i=0;i<num;i++){
        result+=*data;
        data++;
    }
    return result;
}


double average(int *data,int num){
    double sum=0;
    for(int i=0;i<num;i++){
        sum+=*data;
        data++;
    }
    return sum/(double)num;
}

double variance(int *data,double average,int num){
    double sum=0;
    
    for(int i=0;i<num;i++){
        sum+=pow(*data-average, 2.0);
        data++;
    }
    
    return sum/(double)num;
}

void calcFeaturevalue(IplImage* src_img, double* output){
    IplImage *dst_img; //グレースケール画像
    //dst_imgの生成
    dst_img=cvCreateImage(cvGetSize(src_img), IPL_DEPTH_8U, 1);
    
    //src_imgをグレースケールに
    cvCvtColor(src_img, dst_img, CV_BGR2GRAY);
    
    //大津の二値化
    IplImage *thr_img;
    //初期化
    thr_img=cvCreateImage(cvGetSize(src_img), IPL_DEPTH_8U, 1);
    
    //rgb各画像に対して大津の二値化
    cvThreshold (dst_img, thr_img, 0, 255, CV_THRESH_BINARY | CV_THRESH_OTSU);
    
    //アフィン変換でサイズを揃える。
    //アフィン変換後の画像
    IplImage *resize_img=cvCreateImage(cvSize(PAI_WIDTH, PAI_HEIGHT), IPL_DEPTH_8U,1);
    //変換前、変換後の4点を指定
    CvPoint2D32f src_point[4],dst_point[4];
    //左上から時計回りで
    src_point[0]=cvPoint2D32f(0, 0);
    src_point[1]=cvPoint2D32f(thr_img->width, 0);
    src_point[2]=cvPoint2D32f(thr_img->width, thr_img->height);
    src_point[3]=cvPoint2D32f(0, thr_img->height);
    
    dst_point[0]=cvPoint2D32f(0, 0);
    dst_point[1]=cvPoint2D32f(PAI_WIDTH, 0);
    dst_point[2]=cvPoint2D32f(PAI_WIDTH, PAI_HEIGHT);
    dst_point[3]=cvPoint2D32f(0, PAI_HEIGHT);
    
    //変換行列
    CvMat *map_matrix;
    map_matrix=cvCreateMat(3, 3, CV_32FC1);
    cvGetPerspectiveTransform(src_point, dst_point, map_matrix);
    
    //画素を数える
    int tate_gaso[TATE_DIV];
    int yoko_gaso[YOKO_DIV];
    //一応初期化
    for(int i=0;i<TATE_DIV;i++){
        tate_gaso[i]=0;
    }
    
    for(int i=0;i<YOKO_DIV;i++){
        yoko_gaso[i]=0;
    }
    
    //    RGBそれぞれの画像について処理
    //変換行列を用いて画像を変換
    cvWarpPerspective(thr_img, resize_img, map_matrix,CV_INTER_NN + CV_WARP_FILL_OUTLIERS, cvScalarAll (100));
    
    for(int i=0;i<PAI_WIDTH;i++){
        for(int j=0;j<PAI_HEIGHT;j++){
            if((unsigned char)resize_img->imageData[i+j*resize_img->widthStep]==0){
                tate_gaso[i/(PAI_WIDTH/TATE_DIV)]++;
                yoko_gaso[j/(PAI_HEIGHT/YOKO_DIV)]++;
            }
        }
    }
    
    //-----特徴量を出力-----
    //横
    for(int i=0;i<YOKO_DIV;i++){
        //        printf("%d,",yoko_gaso[i]);
        output[i]=yoko_gaso[i];
    }
    //縦
    for(int i=TATE_DIV-1;i>=0;i--){
        //        printf("%d,",tate_gaso[i]);
        output[YOKO_DIV+i] = tate_gaso[i];
    }
    
    //sum temp
    int sum_y = 0;
    int sum_t = 0;
    
    //画素sum
    sum_y =sum(yoko_gaso,YOKO_DIV);
    sum_t =sum(tate_gaso,TATE_DIV);
    //画素sum
    output[YOKO_DIV+TATE_DIV]=sum_t+sum_y;
    //    printf("%d,",sum_y + sum_t);
    
    //sum(横)/sum(縦)
    //printf("%f,",sum_y/(double)sum_t);
    
    //ave
    double ave_tate,ave_yoko;
    ave_tate=average(tate_gaso, TATE_DIV);
    ave_yoko=average(yoko_gaso, YOKO_DIV);
    
    //ave(横)
    output[YOKO_DIV+TATE_DIV+1]=ave_yoko;
    //    printf("%f,",ave_yoko);
    //ave(縦)
    //    printf("%f,",ave_tate);
    output[YOKO_DIV+TATE_DIV+2]=ave_tate;
    //ave(横)/ave(縦)
    //printf("%f,",ave_yoko/ave_tate);
    
    //variance(縦)
    double vari_tate,vari_yoko;
    vari_tate=variance(tate_gaso, ave_tate, TATE_DIV);
    output[YOKO_DIV+TATE_DIV+3]=vari_tate;
    //    printf("%f,",vari_tate);
    //variance(横)
    vari_yoko=variance(yoko_gaso, ave_yoko, YOKO_DIV);
    output[YOKO_DIV+TATE_DIV+4]=vari_yoko;
    
    //    printf("%f,",vari_yoko);
    //variance(横)/variance(縦)
    //printf("%f,",vari_yoko/vari_tate);
    
    //-----特徴量ここまで-----
    //最後に改行を入れるのを忘れない!!!!!
    
    //特徴量の最後にクラスを示して改行
    //    printf("%s",filedir);
    //class
    
}
