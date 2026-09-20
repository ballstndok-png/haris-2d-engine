#include <jni.h>
#include <stdint.h>
#include <string.h>
#include <algorithm>
extern "C" {
int haris_android_create(void);
int haris_android_load_source(const char*src);
void haris_android_set_screen(int w,int h);
void haris_android_set_save_dir(const char*dir);
int haris_android_frame(double dt);
int haris_android_command_count(void);
int haris_android_command(int i,int*type,uint32_t*color,int*fill,float p[8],const unsigned char**blob);
void haris_android_touch(int action,float x,float y,int id);
void haris_android_key(int key,int down);
const char*haris_android_error(void);
int haris_android_logical_width(void);
int haris_android_logical_height(void);
}

extern "C" JNIEXPORT void JNICALL Java_com_haris_engine_NativeBridge_create(JNIEnv*, jclass){ haris_android_create(); }
extern "C" JNIEXPORT jboolean JNICALL Java_com_haris_engine_NativeBridge_loadSource(JNIEnv*env,jclass,jstring source){
    if(!source)return JNI_FALSE;const char*s=env->GetStringUTFChars(source,nullptr);if(!s)return JNI_FALSE;int ok=haris_android_load_source(s);env->ReleaseStringUTFChars(source,s);return ok?JNI_TRUE:JNI_FALSE;
}
extern "C" JNIEXPORT jint JNICALL Java_com_haris_engine_NativeBridge_frame(JNIEnv*,jclass,jfloat dt){ return (jint)haris_android_frame((double)dt); }
extern "C" JNIEXPORT void JNICALL Java_com_haris_engine_NativeBridge_setScreenSize(JNIEnv*,jclass,jint w,jint h){ haris_android_set_screen((int)w,(int)h); }
extern "C" JNIEXPORT void JNICALL Java_com_haris_engine_NativeBridge_setSaveDir(JNIEnv*env,jclass,jstring dir){
    if(!dir){haris_android_set_save_dir(nullptr);return;}const char*s=env->GetStringUTFChars(dir,nullptr);haris_android_set_save_dir(s);env->ReleaseStringUTFChars(dir,s);
}
extern "C" JNIEXPORT jint JNICALL Java_com_haris_engine_NativeBridge_logicalWidth(JNIEnv*,jclass){
    /* command-free getter: read the engine state through a tiny synthetic query is intentionally avoided. */
    return (jint)haris_android_logical_width();
}
extern "C" JNIEXPORT jint JNICALL Java_com_haris_engine_NativeBridge_logicalHeight(JNIEnv*,jclass){ return (jint)haris_android_logical_height(); }
extern "C" JNIEXPORT jint JNICALL Java_com_haris_engine_NativeBridge_getCommands(JNIEnv*env,jclass,jfloatArray data,jintArray meta,jbyteArray blob){
    if(!data||!meta||!blob)return 0;jsize dn=env->GetArrayLength(data),mn=env->GetArrayLength(meta),bn=env->GetArrayLength(blob);int max=std::min((int)(dn/8),(int)(mn/4));int count=std::min(haris_android_command_count(),max);jfloat*dp=env->GetFloatArrayElements(data,nullptr);jint*mp=env->GetIntArrayElements(meta,nullptr);jbyte*bp=env->GetByteArrayElements(blob,nullptr);int blobPos=0;
    for(int i=0;i<count;i++){int type=0,fill=0;uint32_t color=0;float p[8]={0};const unsigned char*text=nullptr;haris_android_command(i,&type,&color,&fill,p,&text);dp[i*8+0]=p[0];dp[i*8+1]=p[1];dp[i*8+2]=p[2];dp[i*8+3]=p[3];dp[i*8+4]=p[4];dp[i*8+5]=p[5];dp[i*8+6]=p[6];dp[i*8+7]=p[7];mp[i*4+0]=type;mp[i*4+1]=(jint)color;mp[i*4+2]=fill;mp[i*4+3]=-1;if(text&&blobPos<bn){size_t left=strlen((const char*)text);size_t cap=(size_t)bn-(size_t)blobPos;if(left+1>cap)left=cap>0?cap-1:0;if(left){memcpy(bp+blobPos,text,left);bp[blobPos+left]=0;mp[i*4+3]=blobPos;blobPos+=(int)left+1;}}}
    env->ReleaseFloatArrayElements(data,dp,0);env->ReleaseIntArrayElements(meta,mp,0);env->ReleaseByteArrayElements(blob,bp,0);return count;
}
extern "C" JNIEXPORT void JNICALL Java_com_haris_engine_NativeBridge_touch(JNIEnv*,jclass,jint action,jfloat x,jfloat y,jint pointerId){haris_android_touch((int)action,(float)x,(float)y,(int)pointerId);}
extern "C" JNIEXPORT void JNICALL Java_com_haris_engine_NativeBridge_key(JNIEnv*,jclass,jint keyCode,jboolean down){haris_android_key((int)keyCode,down?1:0);}
extern "C" JNIEXPORT jstring JNICALL Java_com_haris_engine_NativeBridge_error(JNIEnv*env,jclass){const char*s=haris_android_error();return env->NewStringUTF(s?s:"");}
