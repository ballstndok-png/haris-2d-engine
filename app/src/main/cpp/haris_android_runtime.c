#define HARIS_NO_MAIN 1
#define HARIS_EMBEDDED 1
#define HARIS_ANDROID_GAME 1
#include "haris/haris_android_deps.h"
typedef struct HMemBuf { int owned; void *data; } HMemBuf;

#include "haris/00_android.c"
#include "haris/01_jit_android.c"
#include "haris/02_android.c"
#include "haris/03_natives_android.c"

/* Optional Forge security/file helpers are intentionally unavailable in this Android game subset. */
static int sha256_file(const char*path,char out[65]){(void)path;if(out)out[0]=0;return 0;}
static char* sec_normalize(const char*src){return src?xdup(src):xdup("");}

static char *g_save_dir = NULL;
static VM g_game_vm;
static Fn *g_game_root = NULL;
static Fn *g_game_callback = NULL;
static int g_game_loaded = 0;
static int g_game_running = 1;
static char g_game_error[512] = {0};
static int g_game_logical_w = 1280;
static int g_game_logical_h = 720;
static int g_game_screen_w = 1280;
static int g_game_screen_h = 720;

static void gc_mark_handle(void*p){ if(!p||!heap_is_ptr(p))return; gc_mark_ptr(p); }
static void gc_mark_global_roots(void){
    for(ActiveVM*n=g_active_vms;n;n=n->next)gc_mark_vm_roots(n->vm);
    for(MethodReg*m=g_methods;m;m=m->next){gc_mark_ptr(m);if(m->type)gc_mark_ptr(m->type);if(m->name)gc_mark_ptr(m->name);if(m->fn)gc_mark_fn(m->fn);}
    if(g_struct_names){gc_mark_ptr(g_struct_names);for(size_t i=0;i<g_struct_count;i++)if(g_struct_names[i])gc_mark_ptr(g_struct_names[i]);}
    if(g_tasks){gc_mark_ptr(g_tasks);for(size_t i=0;i<g_task_n;i++)if(g_tasks[i])gc_mark_handle(g_tasks[i]);}
    if(g_game_root)gc_mark_fn(g_game_root);
    if(g_game_callback)gc_mark_fn(g_game_callback);
}
#include "haris/haris_vm_core.cfrag"

static void bindn_android(Env*e,const char*k,Native f){en(e,k,(Value){.t=VNATIVE,.u.native=f});}

#define H2D_MAX_COMMANDS 4096
#define H2D_BLOB_BYTES 262144
#define H2D_TEXT_BYTES 192
enum { H2D_CLEAR=1, H2D_RECT=2, H2D_CIRCLE=3, H2D_LINE=4, H2D_TEXT=5, H2D_SPRITE=6, H2D_SOUND=7, H2D_VIBRATE=8 };
typedef struct { int type; uint32_t color; int fill; float p[8]; int blob_off; } H2DCmd;
static H2DCmd g_cmds[H2D_MAX_COMMANDS]; static int g_cmd_n=0;
static unsigned char g_blob[H2D_BLOB_BYTES]; static int g_blob_n=0;
static void h2d_commands_reset(void){g_cmd_n=0;g_blob_n=0;}
static int h2d_blob_put(const char*s){if(!s)s="";size_t n=strlen(s);if(n>H2D_TEXT_BYTES-1)n=H2D_TEXT_BYTES-1;if(g_blob_n+(int)n+1>H2D_BLOB_BYTES)return -1;int off=g_blob_n;memcpy(g_blob+g_blob_n,s,n);g_blob_n+=(int)n;g_blob[g_blob_n++]=0;return off;}
static int h2d_push(int type,uint32_t color,int fill,const float*p,int blob_off){if(g_cmd_n>=H2D_MAX_COMMANDS)return 0;H2DCmd*c=&g_cmds[g_cmd_n++];memset(c,0,sizeof(*c));c->type=type;c->color=color;c->fill=fill;c->blob_off=blob_off;if(p)memcpy(c->p,p,sizeof(c->p));return 1;}
static int h2d_num(Value v,double*out){if(!isnum(v)||!out)return 0;*out=dn(v);return isfinite(*out);}
static uint32_t h2d_color(Value v){
    if(v.t!=VSTR||!v.u.s||!*v.u.s)return 0xFFFFFFFFu;const char*s=v.u.s;unsigned r=0,g=0,b=0,a=255;size_t n=strlen(s);
    if(s[0]=='#'){s++;n=strlen(s);if(n==6&&sscanf(s,"%02x%02x%02x",&r,&g,&b)==3)return (a<<24)|(r<<16)|(g<<8)|b;if(n==8&&sscanf(s,"%02x%02x%02x%02x",&a,&r,&g,&b)==4)return (a<<24)|(r<<16)|(g<<8)|b;}
    if(!strcasecmp(s,"white"))return 0xFFFFFFFFu;if(!strcasecmp(s,"black"))return 0xFF000000u;if(!strcasecmp(s,"red"))return 0xFFFF4040u;if(!strcasecmp(s,"green"))return 0xFF40FF70u;if(!strcasecmp(s,"blue"))return 0xFF4090FFu;if(!strcasecmp(s,"yellow"))return 0xFFFFFF40u;if(!strcasecmp(s,"cyan"))return 0xFF40FFFFu;if(!strcasecmp(s,"magenta"))return 0xFFFF40FFu;return 0xFFFFFFFFu;
}
static int h2d_key_code(const char*s){
    if(!s||!*s)return -1;if(!strcasecmp(s,"left"))return 21;if(!strcasecmp(s,"right"))return 22;if(!strcasecmp(s,"up"))return 19;if(!strcasecmp(s,"down"))return 20;if(!strcasecmp(s,"space"))return 62;if(!strcasecmp(s,"escape")||!strcasecmp(s,"esc"))return 111;if(!strcasecmp(s,"enter")||!strcasecmp(s,"return"))return 66;if(!strcasecmp(s,"backspace"))return 67;if(!strcasecmp(s,"tab"))return 61;if(!strcasecmp(s,"shift"))return 59;if(!strcasecmp(s,"ctrl")||!strcasecmp(s,"control"))return 113;if(!strcasecmp(s,"alt"))return 57;
    if(strlen(s)==1){char c=(char)tolower((unsigned char)s[0]);if(c>='a'&&c<='z')return 29+(c-'a');if(c>='0'&&c<='9')return 7+(c-'0');}
    for(int i=1;i<=12;i++){char buf[8];snprintf(buf,sizeof(buf),"f%d",i);if(!strcasecmp(s,buf))return 131+i;}return -1;
}

typedef struct { int active,pressed,released; float x,y,dx,dy; } H2DTouch;
static H2DTouch g_touch[10]; static unsigned char g_keys[256];
static double g_last_dt=1.0/60.0,g_last_frame=1.0/60.0,g_game_time=0.0,g_cam_x=0.0,g_cam_y=0.0;
static int g_touch_any(void){for(int i=0;i<10;i++)if(g_touch[i].active)return 1;return 0;}

static Value h2d_clear(VM*,int,Value*);
static Value h2d_window(VM*vm,int n,Value*a){(void)vm;int w=1280,h=720;if(n>=1&&isnum(a[0]))w=(int)fmax(64,fmin(8192,dn(a[0])));if(n>=2&&isnum(a[1]))h=(int)fmax(64,fmin(8192,dn(a[1])));g_game_logical_w=w;g_game_logical_h=h;Value o=vsobj();stput(o.u.st,"__type",vs("android2d_window"));stput(o.u.st,"w",vi(w));stput(o.u.st,"h",vi(h));return o;}
static Value h2d_run(VM*vm,int n,Value*a){(void)vm;if(n<2||n>3||a[1].t!=VFN)return vb(0);g_game_callback=a[1].u.fn;g_game_running=1;if(n==3&&isnum(a[2])){double fps=dn(a[2]);if(fps>=1&&fps<=240)g_last_dt=1.0/fps;}return vb(1);}
static Value h2d_poll(VM*vm,int n,Value*a){(void)vm;(void)a;return n?vb(0):vb(g_game_running);} static Value h2d_is_open(VM*vm,int n,Value*a){(void)vm;(void)a;return n?vb(0):vb(g_game_running);}
static Value h2d_begin(VM*vm,int n,Value*a){if(n!=2||a[1].t!=VSTR)return vb(0);return h2d_clear(vm,1,&a[1]);} static Value h2d_end(VM*vm,int n,Value*a){(void)vm;(void)a;return n?vb(0):vb(1);} static Value h2d_quit(VM*vm,int n,Value*a){(void)vm;(void)a;if(n)return vb(0);g_game_running=0;return vb(1);} static Value h2d_close(VM*vm,int n,Value*a){(void)vm;(void)a;g_game_running=0;return vb(1);}
static Value h2d_clear(VM*vm,int n,Value*a){(void)vm;if(n!=1)return vb(0);h2d_push(H2D_CLEAR,h2d_color(a[0]),1,NULL,-1);return vb(1);}
static Value h2d_rect(VM*vm,int n,Value*a){(void)vm;if(n<5||n>6)return vb(0);double x,y,w,h;if(!h2d_num(a[0],&x)||!h2d_num(a[1],&y)||!h2d_num(a[2],&w)||!h2d_num(a[3],&h)||a[4].t!=VSTR)return vb(0);float p[6]={(float)(x-g_cam_x),(float)(y-g_cam_y),(float)w,(float)h,0,0};int fill=n==6&&a[5].t==VBOOL?a[5].u.b:1;h2d_push(H2D_RECT,h2d_color(a[4]),fill,p,-1);return vb(1);}
static Value h2d_line(VM*vm,int n,Value*a){(void)vm;if(n<5||n>6)return vb(0);double x1,y1,x2,y2,w=1;if(!h2d_num(a[0],&x1)||!h2d_num(a[1],&y1)||!h2d_num(a[2],&x2)||!h2d_num(a[3],&y2)||a[4].t!=VSTR)return vb(0);if(n==6&&isnum(a[5]))w=fmax(1.0,dn(a[5]));float p[6]={(float)(x1-g_cam_x),(float)(y1-g_cam_y),(float)(x2-g_cam_x),(float)(y2-g_cam_y),(float)w,0};h2d_push(H2D_LINE,h2d_color(a[4]),0,p,-1);return vb(1);}
static Value h2d_circle(VM*vm,int n,Value*a){(void)vm;if(n<4||n>5)return vb(0);double x,y,r;if(!h2d_num(a[0],&x)||!h2d_num(a[1],&y)||!h2d_num(a[2],&r)||a[3].t!=VSTR)return vb(0);int fill=n==5&&a[4].t==VBOOL?a[4].u.b:1;float p[6]={(float)(x-g_cam_x),(float)(y-g_cam_y),(float)fabs(r),0,0,0};h2d_push(H2D_CIRCLE,h2d_color(a[3]),fill,p,-1);return vb(1);}
static Value h2d_text(VM*vm,int n,Value*a){(void)vm;if(n<4||n>5)return vb(0);double x,y,size=18;if(!h2d_num(a[0],&x)||!h2d_num(a[1],&y)||a[2].t!=VSTR||a[3].t!=VSTR)return vb(0);if(n==5&&isnum(a[4]))size=fmax(6.0,fmin(128.0,dn(a[4])));int off=h2d_blob_put(a[2].u.s);if(off<0)return vb(0);float p[6]={(float)(x-g_cam_x),(float)(y-g_cam_y),(float)size,0,0,0};h2d_push(H2D_TEXT,h2d_color(a[3]),0,p,off);return vb(1);}
static Value h2d_screen_text(VM*vm,int n,Value*a){(void)vm;if(n<4||n>5)return vb(0);double x,y,size=18;if(!h2d_num(a[0],&x)||!h2d_num(a[1],&y)||a[2].t!=VSTR||a[3].t!=VSTR)return vb(0);if(n==5&&isnum(a[4]))size=fmax(6.0,fmin(128.0,dn(a[4])));int off=h2d_blob_put(a[2].u.s);if(off<0)return vb(0);float p[6]={(float)x,(float)y,(float)size,1,0,0};h2d_push(H2D_TEXT,h2d_color(a[3]),1,p,off);return vb(1);}
static Value h2d_sprite(VM*vm,int n,Value*a){(void)vm;if(n<5||n>6||a[0].t!=VSTR)return vb(0);double x,y,w,h,alpha=1;if(!h2d_num(a[1],&x)||!h2d_num(a[2],&y)||!h2d_num(a[3],&w)||!h2d_num(a[4],&h))return vb(0);if(n==6&&isnum(a[5]))alpha=fmax(0.0,fmin(1.0,dn(a[5])));int off=h2d_blob_put(a[0].u.s);if(off<0)return vb(0);float p[6]={(float)(x-g_cam_x),(float)(y-g_cam_y),(float)w,(float)h,(float)alpha,0};h2d_push(H2D_SPRITE,0xFFFFFFFFu,0,p,off);return vb(1);}
static Value h2d_sprite_frame(VM*vm,int n,Value*a){(void)vm;if(n<7||n>8||a[0].t!=VSTR)return vb(0);double x,y,w,h,frame,cols,rows=0;if(!h2d_num(a[1],&x)||!h2d_num(a[2],&y)||!h2d_num(a[3],&w)||!h2d_num(a[4],&h)||!h2d_num(a[5],&frame)||!h2d_num(a[6],&cols))return vb(0);if(n==8&&isnum(a[7]))rows=fmax(0.0,dn(a[7]));if(cols<1)cols=1;int off=h2d_blob_put(a[0].u.s);if(off<0)return vb(0);float p[8]={(float)(x-g_cam_x),(float)(y-g_cam_y),(float)w,(float)h,(float)frame,(float)cols,(float)rows,0};h2d_push(9,0xFFFFFFFFu,0,p,off);return vb(1);}
static Value h2d_sound(VM*vm,int n,Value*a){(void)vm;if(n<1||n>2||a[0].t!=VSTR)return vb(0);double vol=1;if(n==2&&isnum(a[1]))vol=fmax(0.0,fmin(1.0,dn(a[1])));int off=h2d_blob_put(a[0].u.s);if(off<0)return vb(0);float p[6]={(float)vol,0,0,0,0,0};h2d_push(H2D_SOUND,0,0,p,off);return vb(1);}
static Value h2d_vibrate(VM*vm,int n,Value*a){(void)vm;if(n!=1||!isnum(a[0]))return vb(0);float p[6]={(float)fmax(1.0,fmin(5000.0,dn(a[0]))),0,0,0,0,0};h2d_push(H2D_VIBRATE,0,0,p,-1);return vb(1);}
static Value h2d_camera(VM*vm,int n,Value*a){(void)vm;if(n==0){Value o=va();ap(o.u.a,vf(g_cam_x));ap(o.u.a,vf(g_cam_y));return o;}if(n==2&&isnum(a[0])&&isnum(a[1])){g_cam_x=dn(a[0]);g_cam_y=dn(a[1]);return vb(1);}return vb(0);} static Value h2d_camera_move(VM*vm,int n,Value*a){(void)vm;if(n!=2||!isnum(a[0])||!isnum(a[1]))return vb(0);g_cam_x+=dn(a[0]);g_cam_y+=dn(a[1]);return vb(1);}
static Value h2d_window_size(VM*vm,int n,Value*a){(void)vm;(void)a;if(n!=1)return vn();Value o=va();ap(o.u.a,vi(g_game_logical_w));ap(o.u.a,vi(g_game_logical_h));return o;} static Value h2d_screen_size(VM*vm,int n,Value*a){(void)vm;(void)a;if(n)return vn();Value o=va();ap(o.u.a,vi(g_game_screen_w));ap(o.u.a,vi(g_game_screen_h));return o;}
static Value h2d_dt(VM*vm,int n,Value*a){(void)vm;(void)a;return n?vn():vf(g_last_dt);} static Value h2d_time(VM*vm,int n,Value*a){(void)vm;(void)a;return n?vn():vf(g_game_time);} static Value h2d_fps(VM*vm,int n,Value*a){(void)vm;(void)a;return n?vn():vf(g_last_frame>1e-9?1.0/g_last_frame:0.0);}
static Value h2d_mouse_position(VM*vm,int n,Value*a){(void)vm;(void)a;if(n)return vn();Value o=va();ap(o.u.a,vf(g_touch[0].x+g_cam_x));ap(o.u.a,vf(g_touch[0].y+g_cam_y));return o;} static Value h2d_mouse_down(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VINT)return vb(0);int b=(int)a[0].u.i;return vb(b==1&&g_touch[0].active);}
static Value h2d_key_down(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vb(0);int k=h2d_key_code(a[0].u.s);return vb(k>=0&&k<256&&(g_keys[k]&1));} static Value h2d_key_pressed(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vb(0);int k=h2d_key_code(a[0].u.s);return vb(k>=0&&k<256&&(g_keys[k]&2));} static Value h2d_key_released(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vb(0);int k=h2d_key_code(a[0].u.s);return vb(k>=0&&k<256&&(g_keys[k]&4));}
static int touch_id(Value v){return v.t==VINT?(int)v.u.i:0;} static Value touch_field(VM*vm,int n,Value*a,int f){(void)vm;if(n>1)return vn();int id=n?touch_id(a[0]):0;if(id<0||id>=10)return vn();H2DTouch*t=&g_touch[id];if(f==0)return vb(t->active);if(f==1)return vb(t->pressed);if(f==2)return vb(t->released);if(f==3)return vf(t->x);if(f==4)return vf(t->y);if(f==5)return vf(t->dx);return vf(t->dy);}
static Value h2d_touch_count(VM*vm,int n,Value*a){(void)vm;(void)a;if(n)return vn();int c=0;for(int i=0;i<10;i++)if(g_touch[i].active)c++;return vi(c);} static Value h2d_touch_down(VM*v,int n,Value*a){return touch_field(v,n,a,0);} static Value h2d_touch_pressed(VM*v,int n,Value*a){return touch_field(v,n,a,1);} static Value h2d_touch_released(VM*v,int n,Value*a){return touch_field(v,n,a,2);} static Value h2d_touch_x(VM*v,int n,Value*a){return touch_field(v,n,a,3);} static Value h2d_touch_y(VM*v,int n,Value*a){return touch_field(v,n,a,4);} static Value h2d_touch_dx(VM*v,int n,Value*a){return touch_field(v,n,a,5);} static Value h2d_touch_dy(VM*v,int n,Value*a){return touch_field(v,n,a,6);}
static Value h2d_body(VM*vm,int n,Value*a){(void)vm;if(n<2||n>5||!isnum(a[0])||!isnum(a[1]))return vn();Value o=vsobj();stput(o.u.st,"__type",vs("2d_body"));stput(o.u.st,"x",a[0]);stput(o.u.st,"y",a[1]);stput(o.u.st,"w",n>=3&&isnum(a[2])?a[2]:vf(32));stput(o.u.st,"h",n>=4&&isnum(a[3])?a[3]:vf(32));stput(o.u.st,"vx",vf(0));stput(o.u.st,"vy",vf(0));stput(o.u.st,"gravity",vf(0));stput(o.u.st,"solid",vb(1));stput(o.u.st,"color",n==5&&a[4].t==VSTR?a[4]:vs("#4de1ff"));return o;}
static int h2d_body_get(Value o,const char*k,double*d){if(o.t!=VSTRUCT)return 0;Value v=stget(o.u.st,k);if(!isnum(v))return 0;*d=dn(v);return isfinite(*d);} static Value h2d_body_set(VM*vm,int n,Value*a){(void)vm;if(n!=3||a[0].t!=VSTRUCT||a[1].t!=VSTR)return vb(0);stput(a[0].u.st,a[1].u.s,a[2]);return vb(1);}
static Value h2d_body_step(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VSTRUCT||!isnum(a[1]))return vb(0);double dt=dn(a[1]),x,y,vx,vy,g;if(dt<0||dt>0.25||!h2d_body_get(a[0],"x",&x)||!h2d_body_get(a[0],"y",&y)||!h2d_body_get(a[0],"vx",&vx)||!h2d_body_get(a[0],"vy",&vy))return vb(0);if(!h2d_body_get(a[0],"gravity",&g))g=0;vy+=g*dt;x+=vx*dt;y+=vy*dt;stput(a[0].u.st,"x",vf(x));stput(a[0].u.st,"y",vf(y));stput(a[0].u.st,"vx",vf(vx));stput(a[0].u.st,"vy",vf(vy));return vb(1);}
static Value h2d_body_draw(VM*vm,int n,Value*a){if(n!=1||a[0].t!=VSTRUCT)return vb(0);double x,y,w,h;if(!h2d_body_get(a[0],"x",&x)||!h2d_body_get(a[0],"y",&y)||!h2d_body_get(a[0],"w",&w)||!h2d_body_get(a[0],"h",&h))return vb(0);Value c=stget(a[0].u.st,"color");Value q[6]={vf(x),vf(y),vf(w),vf(h),c,vb(1)};return h2d_rect(vm,6,q);}
static Value h2d_collide(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VSTRUCT||a[1].t!=VSTRUCT)return vb(0);double ax,ay,aw,ah,bx,by,bw,bh;if(!h2d_body_get(a[0],"x",&ax)||!h2d_body_get(a[0],"y",&ay)||!h2d_body_get(a[0],"w",&aw)||!h2d_body_get(a[0],"h",&ah)||!h2d_body_get(a[1],"x",&bx)||!h2d_body_get(a[1],"y",&by)||!h2d_body_get(a[1],"w",&bw)||!h2d_body_get(a[1],"h",&bh))return vb(0);return vb(ax<bx+bw&&ax+aw>bx&&ay<by+bh&&ay+ah>by);}
static Value h2d_resolve(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VSTRUCT||a[1].t!=VSTRUCT)return vb(0);double ax,ay,aw,ah,bx,by,bw,bh,avx=0,avy=0;if(!h2d_body_get(a[0],"x",&ax)||!h2d_body_get(a[0],"y",&ay)||!h2d_body_get(a[0],"w",&aw)||!h2d_body_get(a[0],"h",&ah)||!h2d_body_get(a[1],"x",&bx)||!h2d_body_get(a[1],"y",&by)||!h2d_body_get(a[1],"w",&bw)||!h2d_body_get(a[1],"h",&bh))return vb(0);if(!h2d_body_get(a[0],"vx",&avx))avx=0;if(!h2d_body_get(a[0],"vy",&avy))avy=0;double ox=fmin(ax+aw,bx+bw)-fmax(ax,bx),oy=fmin(ay+ah,by+bh)-fmax(ay,by);if(ox<=0||oy<=0)return vb(0);if(ox<oy){ax+=(ax<bx)?-ox:ox;avx=0;}else{ay+=(ay<by)?-oy:oy;avy=0;}stput(a[0].u.st,"x",vf(ax));stput(a[0].u.st,"y",vf(ay));stput(a[0].u.st,"vx",vf(avx));stput(a[0].u.st,"vy",vf(avy));return vb(1);}
static Value h2d_anim_frame(VM*vm,int n,Value*a){(void)vm;if(n!=3||!isnum(a[0])||!isnum(a[1])||!isnum(a[2]))return vi(0);double t=fmax(0.0,dn(a[0])),fps=fmax(0.0,dn(a[1])),cnt=fmax(1.0,dn(a[2]));long long f=(long long)floor(t*fps);return vi(f%(long long)cnt);}
static Value h2d_distance(VM*vm,int n,Value*a){(void)vm;if(n!=4||!isnum(a[0])||!isnum(a[1])||!isnum(a[2])||!isnum(a[3]))return vn();double dx=dn(a[2])-dn(a[0]),dy=dn(a[3])-dn(a[1]);return vf(sqrt(dx*dx+dy*dy));} static Value h2d_lerp(VM*vm,int n,Value*a){(void)vm;if(n!=3||!isnum(a[0])||!isnum(a[1])||!isnum(a[2]))return vn();double t=fmax(0.0,fmin(1.0,dn(a[2])));return vf(dn(a[0])+(dn(a[1])-dn(a[0]))*t);}
static int valid_save_key(const char*k){if(!k||!*k||strlen(k)>64)return 0;for(const unsigned char*p=(const unsigned char*)k;*p;p++)if(!(isalnum(*p)||*p=='_'||*p=='-'))return 0;return 1;}
static Value h2d_save(VM*vm,int n,Value*a){(void)vm;if(n!=2||a[0].t!=VSTR||a[1].t!=VSTR||!g_save_dir||!valid_save_key(a[0].u.s))return vb(0);char path[PATH_MAX];snprintf(path,sizeof(path),"%s/%s.dat",g_save_dir,a[0].u.s);FILE*f=fopen(path,"wb");if(!f)return vb(0);size_t z=strlen(a[1].u.s);int ok=fwrite(a[1].u.s,1,z,f)==z;fclose(f);return vb(ok);}
static Value h2d_load(VM*vm,int n,Value*a){(void)vm;if(n<1||n>2||a[0].t!=VSTR||!g_save_dir||!valid_save_key(a[0].u.s))return n==2?a[1]:vn();char path[PATH_MAX];snprintf(path,sizeof(path),"%s/%s.dat",g_save_dir,a[0].u.s);FILE*f=fopen(path,"rb");if(!f)return n==2?a[1]:vn();char buf[65536];size_t z=fread(buf,1,sizeof(buf)-1,f);fclose(f);buf[z]=0;return vs(buf);}
static char g_scene_name[128]="main"; static Value h2d_scene(VM*vm,int n,Value*a){(void)vm;if(n!=1||a[0].t!=VSTR)return vb(0);snprintf(g_scene_name,sizeof(g_scene_name),"%s",a[0].u.s);return vb(1);} static Value h2d_current_scene(VM*vm,int n,Value*a){(void)vm;(void)a;return n?vn():vs(g_scene_name);}

static Value native_value(Native f){return (Value){.t=VNATIVE,.u.native=f};}
static void bind_haris_2d_android(VM*vm){
    Value ns=vsobj();
    stput(ns.u.st,"window",native_value(h2d_window));stput(ns.u.st,"run",native_value(h2d_run));stput(ns.u.st,"poll",native_value(h2d_poll));stput(ns.u.st,"is_open",native_value(h2d_is_open));stput(ns.u.st,"begin",native_value(h2d_begin));stput(ns.u.st,"end",native_value(h2d_end));stput(ns.u.st,"clear",native_value(h2d_clear));stput(ns.u.st,"close",native_value(h2d_close));stput(ns.u.st,"quit",native_value(h2d_quit));
    stput(ns.u.st,"rect",native_value(h2d_rect));stput(ns.u.st,"line",native_value(h2d_line));stput(ns.u.st,"circle",native_value(h2d_circle));stput(ns.u.st,"anim_frame",native_value(h2d_anim_frame));stput(ns.u.st,"text",native_value(h2d_text));stput(ns.u.st,"screen_text",native_value(h2d_screen_text));stput(ns.u.st,"sprite",native_value(h2d_sprite));stput(ns.u.st,"sprite_frame",native_value(h2d_sprite_frame));stput(ns.u.st,"sound",native_value(h2d_sound));stput(ns.u.st,"vibrate",native_value(h2d_vibrate));
    stput(ns.u.st,"camera",native_value(h2d_camera));stput(ns.u.st,"camera_move",native_value(h2d_camera_move));stput(ns.u.st,"window_size",native_value(h2d_window_size));stput(ns.u.st,"screen_size",native_value(h2d_screen_size));stput(ns.u.st,"dt",native_value(h2d_dt));stput(ns.u.st,"fps",native_value(h2d_fps));stput(ns.u.st,"time",native_value(h2d_time));
    stput(ns.u.st,"key_down",native_value(h2d_key_down));stput(ns.u.st,"key_pressed",native_value(h2d_key_pressed));stput(ns.u.st,"key_released",native_value(h2d_key_released));stput(ns.u.st,"mouse_position",native_value(h2d_mouse_position));stput(ns.u.st,"mouse_down",native_value(h2d_mouse_down));
    stput(ns.u.st,"touch_count",native_value(h2d_touch_count));stput(ns.u.st,"touch_down",native_value(h2d_touch_down));stput(ns.u.st,"touch_pressed",native_value(h2d_touch_pressed));stput(ns.u.st,"touch_released",native_value(h2d_touch_released));stput(ns.u.st,"touch_x",native_value(h2d_touch_x));stput(ns.u.st,"touch_y",native_value(h2d_touch_y));stput(ns.u.st,"touch_dx",native_value(h2d_touch_dx));stput(ns.u.st,"touch_dy",native_value(h2d_touch_dy));
    stput(ns.u.st,"body",native_value(h2d_body));stput(ns.u.st,"body_set",native_value(h2d_body_set));stput(ns.u.st,"body_step",native_value(h2d_body_step));stput(ns.u.st,"body_draw",native_value(h2d_body_draw));stput(ns.u.st,"collide",native_value(h2d_collide));stput(ns.u.st,"distance",native_value(h2d_distance));stput(ns.u.st,"lerp",native_value(h2d_lerp));
    stput(ns.u.st,"save",native_value(h2d_save));stput(ns.u.st,"load",native_value(h2d_load));stput(ns.u.st,"scene",native_value(h2d_scene));stput(ns.u.st,"current_scene",native_value(h2d_current_scene));
    en(&vm->g,"game2d",ns);
}

static void init_android_vm(VM*vm){
    memset(vm,0,sizeof *vm);vm->module_sandboxed=1;vm->cpu_percent=100;vm->rng_state=0x9e3779b97f4a7c15ULL;vm->memory_auto=1;
    bindn_android(&vm->g,"print",nprint);bindn_android(&vm->g,"len",nlen);bindn_android(&vm->g,"list.append",nappend);bindn_android(&vm->g,"list.push",nappend);bindn_android(&vm->g,"list.set",nlist_set);bindn_android(&vm->g,"list.pop",npop);bindn_android(&vm->g,"list.insert",ninsert);
    bindn_android(&vm->g,"string.substring",nsubstr);bindn_android(&vm->g,"string.replace",nreplace);bindn_android(&vm->g,"string.upper",nupper);bindn_android(&vm->g,"string.lower",nlower);bindn_android(&vm->g,"string.split",nsplit);
    bindn_android(&vm->g,"math.abs",nabs);bindn_android(&vm->g,"math.sqrt",nsqrt);bindn_android(&vm->g,"math.pow",npow);bindn_android(&vm->g,"math.floor",nfloor);bindn_android(&vm->g,"math.ceil",nceil);bindn_android(&vm->g,"math.min",hr_min);bindn_android(&vm->g,"math.max",hr_max);bindn_android(&vm->g,"random.int",hr_rand_int);bindn_android(&vm->g,"random.float",hr_rand_float);bindn_android(&vm->g,"random.int_range",hr_rand_int_range);bindn_android(&vm->g,"random.float_range",hr_rand_float_range);
    bind_haris_2d_android(vm);
}

int haris_android_create(void){if(g_game_loaded)return 1;init_android_vm(&g_game_vm);g_game_running=1;g_game_loaded=0;g_game_error[0]=0;h2d_commands_reset();return 1;}
int haris_android_load_source(const char*src){if(!src)return 0;g_game_error[0]=0;g_game_callback=NULL;g_game_running=1;g_game_vm.jmp_depth=1;g_runtime_vm=&g_game_vm;if(setjmp(g_game_vm.jmp_stack[0])==0){g_game_root=compile(src,"android_game");g_game_vm.jmp_depth=0;g_runtime_vm=NULL;if(!g_game_root){snprintf(g_game_error,sizeof g_game_error,"compile returned null");return 0;}run(&g_game_vm,g_game_root,0,NULL);if(g_game_vm.last_error){snprintf(g_game_error,sizeof g_game_error,"%s",g_game_vm.err_msg);return 0;}g_game_loaded=1;return g_game_callback?1:1;}g_game_vm.jmp_depth=0;g_runtime_vm=NULL;snprintf(g_game_error,sizeof g_game_error,"%s",g_game_vm.err_msg[0]?g_game_vm.err_msg:"Haris compile error");return 0;}
void haris_android_set_screen(int w,int h){if(w>0)g_game_screen_w=w;if(h>0)g_game_screen_h=h;}
void haris_android_set_save_dir(const char*dir){if(g_save_dir){free(g_save_dir);g_save_dir=NULL;}if(dir&&*dir)g_save_dir=strdup(dir);}
int haris_android_frame(double dt){if(!g_game_loaded||!g_game_running)return g_game_running; if(!(dt>0)||dt>0.25)dt=1.0/60.0;g_last_frame=dt;g_last_dt=dt;g_game_time+=dt;h2d_commands_reset();h2d_push(H2D_CLEAR,0xFF000000u,1,NULL,-1);
    if(g_game_callback){Value arg[1]={vf(dt)};run(&g_game_vm,g_game_callback,1,arg);if(g_game_vm.last_error){snprintf(g_game_error,sizeof g_game_error,"%s",g_game_vm.err_msg);g_game_running=0;return 0;}}
    for(int i=0;i<256;i++)g_keys[i]&=1;for(int i=0;i<10;i++){g_touch[i].pressed=0;g_touch[i].released=0;g_touch[i].dx=0;g_touch[i].dy=0;}
    return g_game_running;
}
int haris_android_command_count(void){return g_cmd_n;}
int haris_android_command(int i,int*type,uint32_t*color,int*fill,float p[6],const unsigned char**blob){if(i<0||i>=g_cmd_n)return 0;H2DCmd*c=&g_cmds[i];if(type)*type=c->type;if(color)*color=c->color;if(fill)*fill=c->fill;if(p)memcpy(p,c->p,sizeof(c->p));if(blob)*blob=(c->blob_off>=0&&c->blob_off<g_blob_n)?g_blob+c->blob_off:NULL;return 1;}
void haris_android_touch(int action,float x,float y,int id){if(id<0||id>=10)return;H2DTouch*t=&g_touch[id];if(action==0||action==5){t->active=1;t->pressed=1;t->dx=0;t->dy=0;t->x=x;t->y=y;}else if(action==2||action==6){t->dx=x-t->x;t->dy=y-t->y;t->x=x;t->y=y;t->active=0;t->released=1;}else if(action==1){t->active=1;t->dx=x-t->x;t->dy=y-t->y;t->x=x;t->y=y;}else if(action==3){for(int i=0;i<10;i++)if(g_touch[i].active){g_touch[i].active=0;g_touch[i].released=1;}}
}
void haris_android_key(int key,int down){if(key<0||key>=256)return;if(down)g_keys[key]|=3;else{g_keys[key]&=~1;g_keys[key]|=4;}}
int haris_android_logical_width(void){return g_game_logical_w;}
int haris_android_logical_height(void){return g_game_logical_h;}
const char*haris_android_error(void){return g_game_error;}
