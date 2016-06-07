// Saturation equalizer
// gimp plugin
// to install (on linux):
// for current user:
// gimptool-2.0 --install saturate.c
//and for system-wide installation (as root):
// gimptool-2.0 --install-admin saturate.c
// For more info read enclosed README.txt, or visit
// http://code.google.com/p/satequalizer/
//contact: tiborb95 at gmail dot com, any feedback welcomed


#include <libgimp/gimp.h>
#include <stdio.h>
#include <libgimp/gimpui.h>
#include <string.h>
#include <stdlib.h> //temporary


#define VERSION 0.9.2
#define VERBOSE FALSE
#define DEBUG FALSE
#define THREADS 4
#define RESPONSE_RESET   99
#define RESPONSE_OK      1
#define ITERATE(x,start,end) for(x=start;x<end;x++)
#define BASEPOS(x,y,ch,width) ch*y*width + ch*x
#define UPINTEND 20
#define DOWNINTEND 0
#define DARKUPLIMIT 0.8
#define LIGHTDOWNLIMIT 0.5
#define UNCOUPLED FALSE
#define COUPLED TRUE
#define SQ(x) pow(x,2)
//#define LINEAR 0
#define QUADRATIC 0
#define HSV 1
#define HSL 2
#define YUV 3
#define QUADRSAFE 4
#define SKINSELECTORR 43010
#define SKINSELECTORG 28305
#define SKINSELECTORB 24990
#define FORMULADEFAULT 0
#define VBOXSPACING 3
#define COLWEIGHTDEF 1
#define INNERBOXBORDER 15
#define SLIDERS 6



typedef struct
{
	gfloat boost[7];
	gfloat borders[7];
	gfloat levels[2];
	gfloat cornerbr[4];
	gint formula;
	gboolean coupled;
	gfloat temperature;
	gfloat skin;
	gfloat skincolors[8]; //see skincolorchanged() for content
	gfloat balweight;
	gboolean docolbal;
	gboolean exportlayer;
	gint width;
	gint height;
	gint image_height;
	gint image_width;
	gint channels;
	gfloat skinuplimit;
	gfloat colorbal[3];
	gboolean preview;
	gboolean docornbr;
} MySatVals;
static MySatVals  maindata = {{ 1.0,1.0,1.0,1.0,1.0,1.0,1.0},{},{0,1.0},{0,0,0,0},
FORMULADEFAULT,UNCOUPLED,0,0,{0,0,0,0,0},COLWEIGHTDEF,FALSE,FALSE} ;


typedef struct {
	gint startline;
	gint endline;
	gint id;
} packer;

typedef struct {
	gfloat brightness;
	gfloat saturation;
	gfloat hue;
} basic;

const gchar *title = "Saturation Equalizer (0.9.1)";
gchar *shiftdef= "RGB shift: no change";

static const gfloat maxboosts[]={4.0, 3.75, 3.5, 3.25, 3.0, 2.75};
static guchar *rect_in_guchar,*rect_out_guchar;
static gint sliders[]={0,1,2,3,4,5,6};
static gfloat avgsat[THREADS];
static gfloat maxsat[THREADS];
static gfloat minsat[THREADS];
static const gfloat relborders[7]={0,0.11,0.21, 0.43 ,0.71, 1.43, 14.29}; //=SLIDERS+1
static const gfloat relsaturation[]={0.07, 0.24, 0.55, 0.17, 0.09}; //=number of color modes
GimpRGB RGBvalues[THREADS];	
GimpHSV HSVvalues[THREADS];	
GimpHSL HSLvalues[THREADS];	
GdkColor skincolor ={0,SKINSELECTORR,SKINSELECTORG,SKINSELECTORB};
static basic pixeldata[THREADS];
static gfloat gammed_values[256];
static char labeltextmin [70],labeltextavg [70],labeltextmax [70];
gint32 image_ID,drawable_ID;
static gint rangestat[]={0,0,0,0,0,0,0,0};

//multithreading related
static GThread *gth[THREADS];
static packer thread_data[THREADS];
//GUI-related
gboolean process_image = FALSE;

static GtkWidget *infolabelmin, *infolabelmax, *infolabelavg;

static GtkWidget *slider0,*slider1,*slider2,*slider3,*slider4,*slider5;
static GtkObject *slider0_adj, *slider1_adj,*slider2_adj,*slider3_adj,*slider4_adj,*slider5_adj; 

static GtkWidget *slider_tmpr;
static GtkObject *slider_tmpr_adj;

static GtkWidget *spin_dark, *spin_light;
static GtkObject *spin_dark_adj, *spin_light_adj;
 
static GtkWidget *slider_bal;
static GtkObject *slider_bal_adj;

static GtkWidget *couplebutton;

static GtkObject *spin_maxdiff_adj;

static GtkWidget *combo;

static GtkWidget *slider_skin;
static GtkObject *slider_skin_adj;

static GtkWidget *skinselector;

static GtkWidget *bal_button;
static GtkWidget *bal_label;

static GtkWidget *preview;

static GtkWidget *spin_tl,*spin_tr,*spin_dl,*spin_dr ; 
static GtkWidget *spin_maxdiff; 

static void query (void);
static void run   (const gchar      *name,
                   gint              nparams,
                   const GimpParam  *param,
                   gint             *nreturn_vals,
                   GimpParam       **return_vals);
static void saturate  (GimpDrawable *drawable,GimpPreview *preview);
GimpPlugInInfo PLUG_IN_INFO = { NULL, NULL, query, run };
static gboolean sat_dialog (GimpDrawable *drawable);
static float get_sat_boost (float oldsat);
static void  alg_changed( GtkComboBox *combo, gpointer data );
static void response_callback (GtkWidget *widget,  gint response_id);
static void set_tmpr (gfloat *R, gfloat *G, gfloat *B, gint id);
static void get_bright (gfloat R,gfloat G,gfloat B,gint id);
static void apply_sat_boost(gfloat *R,gfloat *G,gfloat *B,gfloat avg,gfloat sat,gfloat sat_boost,gint id);
static void apply_quad_sat(gfloat* R, gfloat* G, gfloat* B, gfloat avg, gfloat sat_boost);
static gfloat get_quad_bright(gfloat R,gfloat G, gfloat B);
void *block_saturate (void *arg);
static void coupled_changed(void);
static void slider_update (GtkAdjustment *adjustment,gint *slider);
static void skincolorchanged(void);
static gfloat getYULskindist(gfloat R,gfloat G,gfloat B);
static void calibrate(void);
static inline gfloat min3(gfloat x,gfloat y,gfloat z);
static inline gfloat max3(gfloat x,gfloat y,gfloat z);
static void maskdiff(gint x1_abs,gint x2_abs,gint y_abs,gfloat *startdiff,gfloat *enddiff);
void exportwrapper (GimpDrawable *drawable);


//set defaults / initial values 

//gchar *mode1 = "Linear";
const gchar *mode2 = "Quadratic";
const gchar *mode3 = "HSV";
const gchar *mode4 = "HSL";
const gchar *mode5 = "YUV";
const gchar *mode6 = "Safe Quadratic";


MAIN()


static void query (void) {
	static GimpParamDef args[] =   {
  		{ GIMP_PDB_INT32, "run-mode", "Run mode" },
    	{ GIMP_PDB_IMAGE, "image", "Input image"  },
    	{ GIMP_PDB_DRAWABLE,"drawable", "Input drawable" }
    };

	gimp_install_procedure (
    	"plug-in-saturate",
    	title,
    	"Advanced saturation - 6 sliders to manipulate saturation (efectivelly saturation curve). \
    	Also other modifications are available",
    	"Tibor Bamhor",
    	"GPL v.3",
    	"2011",
    	"_Saturation Equalizer",
    	"RGB*",
    	GIMP_PLUGIN,
    	G_N_ELEMENTS (args), 0,
    	args, NULL);

	gimp_plugin_menu_register ("plug-in-saturate",                        
    	"<Image>/Filters/Enhance");
}

void exportwrapper (GimpDrawable *drawable){ 
	maindata.exportlayer=TRUE;
	saturate (drawable,NULL);
	maindata.exportlayer=FALSE;	}

void slider_update (GtkAdjustment *adjustment,int *slider) {
	//CALLED BY: sat_dialog ()
	
	guchar c; 			//iterator
	gfloat max_diff;	//content of spinbox
	const static gboolean debug=FALSE;
	
	//setting value from actual slider
	maindata.boost[*slider]=gtk_adjustment_get_value  (adjustment);
	max_diff=gtk_adjustment_get_value((GtkAdjustment*) spin_maxdiff_adj);
	
	if (debug) printf ("value for slider %.d: %.3f\n", *slider,gtk_adjustment_get_value  (adjustment));
	if (maindata.coupled == UNCOUPLED) return;
	
	//there are 7 values (and 6 sliders) in total
	//here we will start from current slider and go on upward and downward
	//going upward
	for (c= *slider;c<7;c++) {
		//printf ("Doing slider %.d\n",c);
		if (maindata.boost[c+1]>maindata.boost[c]+max_diff) maindata.boost[c+1]=maindata.boost[c]+max_diff;
		if (maindata.boost[c+1]<maindata.boost[c]-max_diff) maindata.boost[c+1]=maindata.boost[c]-max_diff;
		}
	//going downward	
	for (c= *slider;c>=1;c--) {
		//printf ("Doing slider %.d\n",c);
		if (maindata.boost[c-1]>maindata.boost[c]+max_diff) maindata.boost[c-1]=maindata.boost[c]+max_diff;
		if (maindata.boost[c-1]<maindata.boost[c]-max_diff) maindata.boost[c-1]=maindata.boost[c]-max_diff	;	}
		
		//now variables are set is stavals.boost, but we need to update sliders
		gtk_adjustment_set_value((GtkAdjustment*) slider0_adj,maindata.boost[0]);
		gtk_adjustment_set_value((GtkAdjustment*) slider1_adj,maindata.boost[1]);
		gtk_adjustment_set_value((GtkAdjustment*) slider2_adj,maindata.boost[2]);
		gtk_adjustment_set_value((GtkAdjustment*) slider3_adj,maindata.boost[3]);
		gtk_adjustment_set_value((GtkAdjustment*) slider4_adj,maindata.boost[4]);
		gtk_adjustment_set_value((GtkAdjustment*) slider5_adj,maindata.boost[5]);
	
	}
	
void skincolorchanged() {
	//this will find out what color is selected in skinselector and set appropriate values to maindata.skincolor
	//CALLED BY: saturate ()

	gfloat R,G,B,avg,avgyuv,tmp1,tmp2;
	gint Rint,Gint,Bint;
	gint c; 			//iterator
	GimpRGB RGBvaluestmp;	
	GimpHSV HSVvaluestmp;	
	GimpHSL HSLvaluestmp;	

	gtk_color_button_get_color ((GtkColorButton*) skinselector, &skincolor);
	//printf ("Selected colors are: %.d, %.d and %.d\n",skincolor.red,skincolor.green,skincolor.blue);
	R=pow( (float)skincolor.red/65536,1/2.2 );
	G=pow( (float)skincolor.green/65536,1/2.2 );
	B=pow( (float)skincolor.blue/65536,1/2.2 );
	get_bright(R,G,B,0);
	avg=pixeldata[0].brightness;

	maindata.skincolors[0]=R-avg;
	maindata.skincolors[1]=G-avg;
	maindata.skincolors[2]=B-avg;

	//below we calculate also H for HSV and HSV, not needed to have them all though
	Rint=skincolor.red/256;
	Gint=skincolor.green/256;
	Bint=skincolor.blue/256;
	RGBvaluestmp.r=Rint; RGBvaluestmp.g=Gint; RGBvaluestmp.b=Bint;
	//HSV
	gimp_rgb_to_hsv (&RGBvaluestmp,&HSVvaluestmp);
	maindata.skincolors[3]=HSVvaluestmp.h;
	//HSL
	gimp_rgb_to_hsl (&RGBvaluestmp,&HSLvaluestmp);
	maindata.skincolors[4]=HSLvaluestmp.h;     

	//YUV
	avgyuv =  0.299*R + 0.587*G + 0.114*B;
	tmp1 = -0.147*R - 0.289*G + 0.436*B;
	tmp2 =  0.615*R - 0.515*G - 0.100*B;
	maindata.skincolors[5]=avgyuv;    
	maindata.skincolors[6]=tmp1;    
	maindata.skincolors[7]=tmp2;    
	if (VERBOSE) ITERATE(c,0,8) {
    	printf("Skincolor (%.d) : %.3f\n",c,maindata.skincolors[c]);}
    
    }


gfloat getYULskindist(gfloat R,gfloat G,gfloat B){
	//this will calculate skindist distance in YUV colorspace
	//CALLED BY: *block_saturate()
	
	gfloat tmp1,tmp2,skindist;
	
	//avgyuv =  0.299*R + 0.587*G + 0.114*B;
   	tmp1 = -0.147*R - 0.289*G + 0.436*B;
   	tmp2 =  0.615*R - 0.515*G - 0.100*B;
   	
   	skindist=pow( (SQ(tmp1 -maindata.skincolors[6]) + SQ(tmp2 - maindata.skincolors[7])) , 0.5);
   	return skindist;
   	}
	

static void run (const gchar      *name,
     			gint              nparams,
     			const GimpParam  *param,
     			gint             *nreturn_vals,
     			GimpParam       **return_vals){
	static GimpParam  values[1];
	GimpPDBStatusType status = GIMP_PDB_SUCCESS;
	GimpRunMode       run_mode;
	GimpDrawable     *drawable;

	/* Setting mandatory output values */
	*nreturn_vals = 1;
	*return_vals  = values;

	values[0].type = GIMP_PDB_STATUS;
	values[0].data.d_status = status;

  image_ID = param[1].data.d_image; //<- zistujem imageID
  if (VERBOSE) printf("Image ID is: %d\n", image_ID); 

	// Getting run_mode - we won't display a dialog if
	// we are in NONINTERACTIVE mode
  	run_mode = param[0].data.d_int32;

  	/*  Get the specified drawable  */
	drawable = gimp_drawable_get (param[2].data.d_drawable);


    switch (run_mode) {
    	case GIMP_RUN_INTERACTIVE:
            /* Get options last values if needed */
            gimp_get_data ("plug-in-saturate", &maindata);

            /* Display the dialog */
            if (! sat_dialog (drawable)) return;
            break;

		case GIMP_RUN_NONINTERACTIVE:
            if (nparams != 4)  status = GIMP_PDB_CALLING_ERROR;
            //if (status == GIMP_PDB_SUCCESS) maindata.value1 = param[3].data.d_int32;
            //???????????????
            break;

		case GIMP_RUN_WITH_LAST_VALS:
            /*  Get options last values if needed  */
            gimp_get_data ("plug-in-saturate", &maindata);
            break;

		default:
            break;
        }

	//gimp_progress_init ("Saturating...");

	saturate (drawable,NULL);


	gimp_displays_flush ();
	gimp_drawable_detach (drawable);

	return;
}


	
void coupled_changed(void){
	//CALLED BY: sat_dialog()
	maindata.coupled=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (couplebutton));
	if (maindata.coupled) 	gtk_widget_set_sensitive(spin_maxdiff,TRUE);
	else gtk_widget_set_sensitive(spin_maxdiff,FALSE);
	if (VERBOSE) printf("ck_changed %d\n",gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (couplebutton)) );
	}


void calibrate() {
	//takes rect_in_guchar and calculate average RGB values
	guint x,y,iterator,count;
	gfloat Rsum,Gsum,Bsum,avg;
	gint basepos;
	gint width,height,channels;
	char collabeltext [50];
	guchar *rect_preview;
	
	iterator =2;
	Rsum=0;Gsum=0;Bsum=0;count=0;	
	
	rect_preview= gimp_zoom_preview_get_source (GIMP_ZOOM_PREVIEW (preview), &width,
			 &height, &channels);
	
	for(x=0;x<width;x=x+iterator) {
		for(y=0;y<height;y=y+iterator) {
			//printf ("Doing %.d x %.d\n",x,y);	
			basepos=BASEPOS(x,y,channels,width);
			Rsum=Rsum+gammed_values[rect_preview[basepos  ]];
			Gsum=Gsum+gammed_values[rect_preview[basepos+1]];
			Bsum=Bsum+gammed_values[rect_preview[basepos+2]];
			count=count+1;}}
	
	g_free (rect_preview);	
	maindata.colorbal[0]=(gfloat)Rsum/count;
	maindata.colorbal[1]=(gfloat)Gsum/count;
	maindata.colorbal[2]=(gfloat)Bsum/count;
	get_bright(maindata.colorbal[0],maindata.colorbal[1],maindata.colorbal[2],0);
	//avg=pixeldata[0].brightness;
	avg=(maindata.colorbal[0] + maindata.colorbal[1] + maindata.colorbal[2]) / 3.0;
	maindata.colorbal[0]=maindata.colorbal[0]-avg;
	maindata.colorbal[1]=maindata.colorbal[1]-avg;
	maindata.colorbal[2]=maindata.colorbal[2]-avg;
	sprintf(collabeltext, "RGB shift: %.3f, %.3f, %.3f",maindata.colorbal[0],
		maindata.colorbal[1],maindata.colorbal[2]);
	gtk_label_set_label(GTK_LABEL(bal_label),collabeltext ); 
	maindata.docolbal=TRUE;
	if (VERBOSE) printf ("Calibrating values: %.3f, %.3f, %.3f\n",Rsum/count,Gsum/count,Bsum/count);
}


gfloat get_sat_boost (float oldsat) {
	//CALED BY: *block_saturate()
	
	gfloat satboost,weight;
	guchar i;
	//const gboolean debug=FALSE;
	
	//iterating over values
	for (i=1;i<SLIDERS+1;i++) {
		if (oldsat<maindata.borders[i]) {
			weight=(oldsat-maindata.borders[i-1])/(maindata.borders[i]-maindata.borders[i-1]);
			satboost=( maindata.boost[i-1] * (1-weight) +  maindata.boost[i] * weight);
			if (DEBUG) rangestat[i-1]+=1;
			return satboost;}}
	if (DEBUG) printf ("get_sat_boost - something is wrong, failed to remap old saturation: %.f\n",oldsat);
	return 1;} //this should not happen


void  alg_changed( GtkComboBox *combo, gpointer data ) {
	//CALLED BY: sat_dialog()
    
    if ( strncmp((gtk_combo_box_get_active_text( combo )),mode2,5) == 0) {
    	maindata.formula=QUADRATIC;}
    if ( strncmp((gtk_combo_box_get_active_text( combo )),mode3,3) == 0) {
    	maindata.formula=HSV;}  	
    if ( strncmp((gtk_combo_box_get_active_text( combo )),mode4,3) == 0) {
    	maindata.formula=HSL;}
    if ( strncmp((gtk_combo_box_get_active_text( combo )),mode5,3) == 0) {
    	maindata.formula=YUV;  	}
    if ( strncmp((gtk_combo_box_get_active_text( combo )),mode6,3) == 0) {
    	maindata.formula=QUADRSAFE;  	}
    	
    if (VERBOSE) printf (" Selected color mode: %1d\n",maindata.formula);
    	
    	}

inline gfloat max3(gfloat x,gfloat y,gfloat z) {
	if (x < y) x=y;
	if (x < z) x=z;
	return x;}
	
inline gfloat min3(gfloat x,gfloat y,gfloat z) {
	if (x > y) x=y;
	if (x > z) x=z;
	return x;}


static void apply_sat_boost(gfloat *R,gfloat *G,gfloat *B,gfloat avg,gfloat sat,gfloat sat_boost,gint id) {
	//this applies saturation boost to rgb values
	//CALLED BY: *block_saturate	
	
	gfloat tmp1,tmp2;
	
	if ( maindata.formula == QUADRATIC) {
		apply_quad_sat(R,G,B,avg,sat_boost);
       	}
    else if ( maindata.formula == QUADRSAFE) {
		apply_quad_sat(R,G,B,avg,sat_boost);
		//if (max3(*R,*G,*B)>1) printf ("max RGB exceeding 1: %.3f\n",max3(*R,*G,*B));
		if (min3(*R,*G,*B)<0) {  // UNOPTIMIZED !!!
			*R=*R-min3(*R,*G,*B);
			*G=*G-min3(*R,*G,*B);
			*B=*B-min3(*R,*G,*B);}
		if (max3(*R,*G,*B)>1) {  // UNOPTIMIZED !!!
			*R=*R-max3(*R,*G,*B)+1;
			*G=*G-max3(*R,*G,*B)+1;
			*B=*B-max3(*R,*G,*B)+1;}
       	}		
	else if ( maindata.formula == HSV) {
		RGBvalues[id].r=*R; RGBvalues[id].g=*G; RGBvalues[id].b=*B;
		gimp_rgb_to_hsv (&RGBvalues[id],&HSVvalues[id]);
		HSVvalues[id].s=MIN(1,sat*sat_boost);
		gimp_hsv_to_rgb(&HSVvalues[id],&RGBvalues[id]);
		*R=RGBvalues[id].r; *G=RGBvalues[id].g; *B= RGBvalues[id].b;}
	else if ( maindata.formula == HSL) {
		RGBvalues[id].r=*R; RGBvalues[id].g=*G; RGBvalues[id].b=*B;
		gimp_rgb_to_hsl (&RGBvalues[id],&HSLvalues[id]);
		HSLvalues[id].s=MIN(1,sat*sat_boost);
		gimp_hsl_to_rgb(&HSLvalues[id],&RGBvalues[id]);
		*R=RGBvalues[id].r; *G=RGBvalues[id].g; *B= RGBvalues[id].b;}
	else if ( maindata.formula == YUV) {
   		tmp1 = -0.147**R - 0.289**G + 0.436**B;
   		tmp2 =  0.615**R - 0.515**G - 0.100**B;
		//calculating new UV values
		tmp1=tmp1*sat_boost; //u
		tmp2=tmp2*sat_boost; //v
		//calculating new UV values
		*R = avg + 1.140*tmp2;
   		*G = avg - 0.395*tmp1 - 0.581*tmp2;
   		*B = avg + 2.032*tmp1;}	
	else {
		printf ("apply_sat_boost(): Unknown color mode, quitting\n");
		exit(1);}
    return ;}


static void apply_quad_sat(gfloat* R, gfloat* G, gfloat* B, gfloat avg_orig, gfloat sat_boost_orig){
	//CALLED BY: apply_sat_boost()
	
	gfloat R_local,G_local, B_local, cur_avg,sat_boost;
	gfloat avg_diff;
	guchar c; 		//iterator

	avg_diff=0;
	sat_boost=sat_boost_orig;

	ITERATE(c,0,3){

		R_local=avg_orig + (*R-avg_orig)*sat_boost + avg_diff;
		G_local=avg_orig + (*G-avg_orig)*sat_boost + avg_diff;    	
		B_local=avg_orig + (*B-avg_orig)*sat_boost + avg_diff;
    
     	cur_avg=get_quad_bright(R_local,G_local,B_local);  
   		avg_diff=avg_diff - (cur_avg - avg_orig);}
    
    *R=R_local;*G=G_local;*B=B_local;
}

gfloat get_quad_bright(gfloat R,gfloat G, gfloat B) {
	//calculates brightness for quadratic modes
	//CALLED BY: get_bright()
	
	gfloat partial1,partial2,partial3,sum;
	
	if (R<0 || R>1) partial1 =0.241 * R;
	else  partial1 =0.241*SQ(R);
	if (G<0 || G>1) partial2 =0.691 * G;
	else  partial2 =0.691*SQ(G);	
	if (B<0 || B>1) partial3 =0.068 * B;
	else  partial3 =0.068*SQ(B);	
	
	sum= partial1+partial2+partial3	;
	//if (sum<0) sum= -sum;
	//else sum=pow (sum,(gfloat) 1/2);
	if (sum>0) sum=pow (sum,(gfloat) 1/2);
	//printf ("sum :%.3f\n",sum);
	
	return sum;}

void maskdiff(gint x1_abs, gint x2_abs,gint y_abs,gfloat *startdiff,gfloat *enddiff) {
	//calculated change in brightness due to per-corner brightening
	//calculating change for 0,y and maindata.image_width,y
	
	*startdiff=maindata.cornerbr[0]*(1.0-(gfloat)y_abs/maindata.image_height) + maindata.cornerbr[2]*(gfloat)y_abs/maindata.image_height;
	*enddiff  =maindata.cornerbr[1]*(1.0-(gfloat)y_abs/maindata.image_height) + maindata.cornerbr[3]*(gfloat)y_abs/maindata.image_height;
}
	

static void get_bright (gfloat R,gfloat G,gfloat B,gint id){
	//CALLED BY:set_tmpr(), *block_saturate(), skincolorchanged()
	
	gfloat avg, tmp1,tmp2;
	
	avg=0;tmp1=0;tmp2=0; //just to have it initialised
	
	if ( maindata.formula == QUADRATIC || maindata.formula == QUADRSAFE ) {
		pixeldata[id].brightness = get_quad_bright(R,G,B);
		avg=pixeldata[id].brightness;
		pixeldata[id].saturation=( ABS(avg-R)+ABS(avg-G)+ABS(avg-B) ) / 3;
		//if (id == 0) printf ("sat. %.3f\n",pixeldata[id].saturation);
		}		
	else if ( maindata.formula == HSV) {
		RGBvalues[id].r=R; RGBvalues[id].g=G; RGBvalues[id].b=B;
		gimp_rgb_to_hsv (&RGBvalues[id],&HSVvalues[id]);
		pixeldata[id].brightness=HSVvalues[id].v;
		pixeldata[id].saturation=HSVvalues[id].s;
		pixeldata[id].hue=HSVvalues[id].h;}
	else if ( maindata.formula == HSL) {
		RGBvalues[id].r=R; RGBvalues[id].g=G; RGBvalues[id].b=B;
		gimp_rgb_to_hsl (&RGBvalues[id],&HSLvalues[id]);
		pixeldata[id].brightness=HSLvalues[id].l;
		pixeldata[id].saturation=HSLvalues[id].s;
		pixeldata[id].hue=HSLvalues[id].h;}	
	else if ( maindata.formula == YUV) {
		avg =  0.299*R + 0.587*G + 0.114*B;
   		tmp1 = -0.147*R - 0.289*G + 0.436*B;
   		tmp2 =  0.615*R - 0.515*G - 0.100*B;
		pixeldata[id].brightness=avg;
		pixeldata[id].saturation= pow (SQ(tmp1)+SQ(tmp2) , (gfloat) 1/2);}	
			
	else {
		printf ("get_bright(): Unknown color mode: %.d, quitting\n",maindata.formula );
		exit(1);}
		
	if ( DEBUG && ( ! (pixeldata[id].saturation == pixeldata[id].saturation) ) ) 
		//printf ("Got nan for values %.3f, %.3f, %.3f\n", R,G,B);
    return ;}


static void set_tmpr (gfloat *R, gfloat *G, gfloat *B,gint id) {
	//CALLED BY: *block_saturate()
	
	gfloat avg,diff;

	get_bright(*R,*G,*B, id);
	avg=pixeldata[id].brightness;
	*R=*R+0.12*maindata.temperature;
	*G=*G+0.05*maindata.temperature;
	*B=*B-0.17*maindata.temperature;
	get_bright(*R,*G,*B, id);
	diff=avg -pixeldata[id].brightness;
	*R=*R+diff;
	*G=*G+diff;	
	*B=*B+diff;	
	get_bright(*R,*G,*B, id);
	diff=avg -pixeldata[id].brightness;
	*R=*R+diff;
	*G=*G+diff;	
	*B=*B+diff;	
}


void *block_saturate (void *arg) {
	//CALLED BY: saturate()
	//this will saturate part of image, it is function that will run concurently in few instances
	
	packer *data;
	gfloat avg,sat_boost;
    gfloat R,G,B;
    guint Rint,Gint,Bint;
    gint basepos,x,y;
    gfloat sat,bright_diff;//,maskweight,maskdarken;
    guint count;
    gfloat skindist, weight;
    gint x1_abs,x2_abs,y_abs; //position of pixel in entire image (relevant for previews)
    gfloat startdiff,enddiff; //changes due to mask
     
       
    data=(packer *) arg;
    
    maxsat[data->id]=0;
    avgsat[data->id]=0;
    minsat[data->id]=1;
    count=0;


	ITERATE(y,0,maindata.height) {
	if (maindata.docornbr) {
		if (maindata.preview) {
			gimp_preview_untransform(GIMP_PREVIEW (preview),data->startline,y, &x1_abs, &y_abs);
			gimp_preview_untransform(GIMP_PREVIEW (preview),data->endline,y, &x2_abs, &y_abs);}
		else {
			y_abs=y;
			x1_abs=data->startline;
			x2_abs=data->endline;}
		maskdiff(x1_abs,x2_abs,y_abs,&startdiff,&enddiff);}
		
		ITERATE(x,data->startline,data->endline) { 
	
			basepos=BASEPOS(x,y,maindata.channels,maindata.width);
       	
			//getting INITAIL values (gammed)     		
      		R=gammed_values[rect_in_guchar[basepos]];
			G=gammed_values[rect_in_guchar[basepos+1]];
			B=gammed_values[rect_in_guchar[basepos+2]];;	
 			if (DEBUG && ! (R==R) ) printf ("Got nan after after gamming, values %.3f, %.3f, %.3f\n", R,G,B);
			
			//doing color balance
			if (maindata.docolbal) {
				R=R-maindata.colorbal[0]*maindata.balweight;
				G=G-maindata.colorbal[1]*maindata.balweight;
				B=B-maindata.colorbal[2]*maindata.balweight;}

 			if (DEBUG && ! (B==B) ) printf ("Got nan after color balance for values %.3f, %.3f, %.3f\n", R,G,B);
 			
 			//statistics for infobox
 			if ( maxsat[data->id]<pixeldata[data->id].saturation) maxsat[data->id]=pixeldata[data->id].saturation; 
 			if ( minsat[data->id]>pixeldata[data->id].saturation) minsat[data->id]=pixeldata[data->id].saturation; 
 			avgsat[data->id]=avgsat[data->id]+pixeldata[data->id].saturation;
 			count=count + 1;
      		
      		
      		//MODIFYING overall BRIGHTNESS due to brightness spinboxes
      		//change
      		get_bright(R,G,B,data->id);
      		avg=pixeldata[data->id].brightness;
      		if (DEBUG && ! (avg == avg) ) printf ("avg during bright modification: %.3f, RGB: %.3f, %.3f ,%.3f\n",
      			 avg, R, G,B);
      		bright_diff=maindata.levels[0] + (-maindata.levels[0] + maindata.levels[1]) * avg-avg;
 			R=R+bright_diff;	G=G+bright_diff;	B=B+bright_diff;
 			avg=avg+bright_diff;  //NOT acurate for quadratic
 			//if (avg>0.6) maskdarken=MASKDARKEN(DARKRATIOWEAK,avg);				
 			if (DEBUG && ! (R==R) ) printf ("Got nan after bright change for values %.3f, %.3f, %.3f, bright_diff: %.3f\n"
 			, R,G,B, bright_diff);
 			  
 			//doing per corner brightness modification
 			if (maindata.docornbr) {
				bright_diff=(1.0 - (gfloat)x/maindata.width ) * startdiff +  (gfloat)x/maindata.width*enddiff;
 				R=R+bright_diff;	G=G+bright_diff;	B=B+bright_diff;}
 			
 			     		
      		//SHIFTING RGB values due to TEMPERATURE spinboxes
      		if ( ! maindata.temperature == 0 ) 	set_tmpr(&R,&G,&B,data->id);

			if (DEBUG && ! (R==R)) printf ("Got nan after temperature for values %.3f, %.3f, %.3f\n", R,G,B);


      		//calculating initial BRIGHTNESS and SATURATION
      		get_bright(R,G,B,data->id);
      		avg=pixeldata[data->id].brightness;
      		sat=pixeldata[data->id].saturation;

      		
      		//calculating SATURATION CHANGE
      		sat_boost= get_sat_boost (sat);
      		//if (data->id == 0) printf("Oldsat: %.3f, satboost: %.3f, direct result: %.3f\n",sat,sat_boost,get_sat_boost (sat));
      		
      		//modifying SATURATION BOOST if colorprotect
      		if ( ! (maindata.skin==0) ) {
      			if (maindata.formula == QUADRATIC || maindata.formula == QUADRSAFE) {
      				skindist=pow( (SQ(R-avg-maindata.skincolors[0]) + \
      				SQ(G-avg-maindata.skincolors[1]) + SQ(B-avg-maindata.skincolors[2])) , 0.5);}
      			if  (maindata.formula == HSV) skindist=ABS(maindata.skincolors[3]-pixeldata[data->id].hue);
      			else if  (maindata.formula == HSV) skindist=ABS(maindata.skincolors[4]-pixeldata[data->id].hue);
      			//for now for YUL colorspace standard algorithm is used
      			else skindist=getYULskindist(R,G,B);
      			
      			//printf ("Skin distance: %.3f\n",skindist);

      			if ( maindata.skin > 0 && skindist > maindata.skinuplimit) {
					sat_boost =1;
					;}
				else if (maindata.skin > 0 && skindist > maindata.skin) {
					weight=(maindata.skinuplimit-skindist)/(maindata.skinuplimit-maindata.skin);
      				sat_boost=sat_boost*weight+(1-weight);}					
				else if (maindata.skin > 0 && skindist >0) {
					//doing nothing 
					;}
					
      			else if (maindata.skin < 0 && skindist > ABS(maindata.skinuplimit)) {
					//doing nothing
					;}
      			else if (maindata.skin < 0  &&  skindist > ABS(maindata.skin)){
					weight=(skindist - maindata.skinuplimit) / (maindata.skin - maindata.skinuplimit);
      				sat_boost=sat_boost*weight+(1-weight);}
      			else if (maindata.skin < 0  &&  skindist  > 0) {
					sat_boost =1;
					;}
				else printf ("Unrecognized color protection range\n");
      			
   			
				}
      		
			//modify saturation    			   		      		
      		apply_sat_boost(&R,&G,&B,avg,sat,sat_boost,data->id);
       		//printf("Boosted RGB %.3f, %.3f, %.3f\n",R,G,B);
       		R=pow(R,2.2) * 255;
       		G=pow(G,2.2) * 255;
       		B=pow(B,2.2) * 255;       	
       		//printf("Ungammed RGB %.3f, %.3f, %.3f\n",R,G,B);

       		//saving calculated values
       		Rint=(int)MAX(MIN(ROUND(R),255),0);
       		Gint=(int)MAX(MIN(ROUND(G),255),0);
       		Bint=(int)MAX(MIN(ROUND(B),255),0);
       		//printf("New RGB %i, %i, %i\n",Rint,Gint,Bint);
        	rect_out_guchar[basepos + 0] = Rint;
        	rect_out_guchar[basepos + 1] = Gint;      	
        	rect_out_guchar[basepos + 2] = Bint; 
        	if (maindata.channels == 4) rect_out_guchar[basepos + 3] =rect_in_guchar[basepos + 3 ]; //alpha
	
	} //end of iteration over pixels in a column
      	if (data->id ==0 && y % 10 == 0 && !maindata.preview)
        	gimp_progress_update ((gdouble) y / (gdouble) maindata.height);
   } //end of iterations over all columns
   
	avgsat[data->id] = (gfloat) avgsat[data->id] / count;
   
	return NULL;
} // end of *block_saturate

void export_to_layer()
{
	GimpDrawable *new_layer;
	guint height,width,layer_ID;
	guint x;
	GimpPixelRgn rgn_out_nl;
	guchar *rect_out_nl;

	
	if (VERBOSE) printf(" Starting export_to_layer() function...\n");
	width=gimp_image_width(image_ID);
	height=gimp_image_height(image_ID);
	
	//creating and attaching new layer
	if (maindata.channels==3)
		layer_ID = gimp_layer_new (image_ID, "SatEq", width, height, GIMP_RGB_IMAGE, 100.0,  GIMP_NORMAL_MODE);
	else if  (maindata.channels==4)
		layer_ID = gimp_layer_new (image_ID, "SatEq", width, height, GIMP_RGBA_IMAGE, 100.0,  GIMP_NORMAL_MODE);
	else printf ("Unsupported number of channels\n");
	new_layer = gimp_drawable_get (layer_ID);
	gimp_image_add_layer (image_ID,layer_ID,-1);
	
	rect_out_nl      = g_new (guchar, maindata.width * maindata.height * maindata.channels);	
		
	//initiating rgn_out
	gimp_pixel_rgn_init (&rgn_out_nl, new_layer, 0, 0, width, height, FALSE, TRUE);

	//populating rgn_out with data
	ITERATE(x,0,width*height*maindata.channels) rect_out_nl[x]=rect_out_guchar[x];
    
	//nahratie udajov do rgn_out
    gimp_pixel_rgn_set_rect (&rgn_out_nl, rect_out_nl,0, 0,width,height);

	g_free (rect_out_nl);
	
	gimp_drawable_flush (new_layer);
	gimp_drawable_merge_shadow (new_layer->drawable_id, TRUE);
	gimp_drawable_update (new_layer->drawable_id,0, 0, width, height);
	gimp_displays_flush ();
	gimp_progress_end();
	//g_free(new_layer);
}	


static void saturate (GimpDrawable *drawable,GimpPreview *preview) {
	//CALLED BY: sat_dialog()

	gint        x1, y1, x2, y2; 		//x,y from image not area
	GimpPixelRgn rgn_in, rgn_out;
 	//gint        width, height;
 	gint 		c;
 	gint 		blocksize,t;
 	gfloat 		finalmaxsat,finalavgsat,finalminsat;



	finalmaxsat=0;finalavgsat=0,finalminsat=1;
	
	if (VERBOSE) printf ("===Starting saturate()\n");

	for (c=0;c<256;c++) { //to speed up conversion from raw RGB to gammed values
		gammed_values[c]=pow((gfloat)c/255,1/2.2);}
	
	//get values needed for skincolor protection
	if (maindata.skin>0) skincolorchanged();
	
	//setting docornbr
	if (maindata.cornerbr[0] ==0 && maindata.cornerbr[1] ==0 &&  maindata.cornerbr[2] ==0  && maindata.cornerbr[3] ==0)
		maindata.docornbr = FALSE;
	else 
		maindata.docornbr = TRUE;
		
 	//make sure g_thread is running
 	//if( !g_thread_supported() )  g_thread_init(NULL); //g_thread_init is no longer needed
 	if( !g_thread_supported() ) {
     	printf("g_thread NOT supported!!!\n");
     	exit (1);}

	if (!preview) gimp_progress_init ("Saturating...");

	// getting coordinates and preview widget if preview mode, + initialising memory
	if (preview){
		rect_in_guchar= gimp_zoom_preview_get_source (GIMP_ZOOM_PREVIEW (preview), &maindata.width,
			 &maindata.height, &maindata.channels);
		maindata.preview=TRUE;
		//getting data for whole image
		gimp_drawable_mask_bounds (drawable->drawable_id,&x1, &y1,&x2, &y2);
		maindata.image_width = x2 - x1;
		maindata.image_height = y2 - y1;
		/* Initialise memory for rect_out_guchar*/		
		rect_out_guchar = g_new (guchar, maindata.channels * maindata.width * maindata.height);	
		}
    else {
		gimp_drawable_mask_bounds (drawable->drawable_id,&x1, &y1,&x2, &y2);
		maindata.width = x2 - x1;
		maindata.height = y2 - y1;
		maindata.preview=FALSE;
	 	maindata.channels = gimp_drawable_bpp (drawable->drawable_id);		
		
		/* Initialise memory for rects*/
		rect_in_guchar  =   g_new (guchar, maindata.channels  * maindata.width * maindata.height); 
		rect_out_guchar   = g_new (guchar, maindata.channels * maindata.width * maindata.height);
		}
         


  //initializing pixelrgn for input and output row
    if ( ! maindata.preview) {
		gimp_pixel_rgn_init (&rgn_in,
						   drawable,
						   x1, y1,
						   maindata.width, maindata.height,
						   FALSE, FALSE); 
		gimp_pixel_rgn_init (&rgn_out,
						   drawable,
						   x1, y1,
						   maindata.width, maindata.height,
						   preview == NULL, TRUE);}


	//loading all data from image
	if (!maindata.preview) gimp_pixel_rgn_get_rect (&rgn_in,rect_in_guchar,x1, y1,maindata.width,maindata.height);
	//if (maindata.preview) printf ("Preview mode\n");

    //iterating and changing borders
    ITERATE(c,0,SLIDERS+1) {
       	maindata.borders[c]=relborders[c]*relsaturation[maindata.formula];
       	if (DEBUG) printf ("   Setting saturation borders #%1d: %3f\n",c,maindata.borders[c]) ;}
	
	//for case skinprotect is on
	if (maindata.skin>0) maindata.skinuplimit=MAX(maindata.skin*2.0,maindata.skin+0.05);

	if (DEBUG) ITERATE(c,0,7) rangestat[c]=0;
	
	//preparing and starting threading  
	if (VERBOSE) printf (" Initiating threads..\n");
	blocksize=maindata.width/THREADS;
	ITERATE(t,0,THREADS) { 
		if (VERBOSE) printf ("  Thread %1d\n",t);
		//preparing data for function
		if     (t==0) thread_data[0].startline=0;
		else   thread_data[t].startline=thread_data[t-1].endline;
		if (t<THREADS-1) thread_data[t].endline=MIN( thread_data[t].startline+blocksize,maindata.width);
		else thread_data[t].endline=maindata.width;
		thread_data[t].id=t;
		//if (VERBOSE) printf ("Thread %.d / %.d started\n",t,thread_data[t].id);
		gth[t]=g_thread_new( NULL, (GThreadFunc) block_saturate,&thread_data[t]); }
	
	//if (VERBOSE) printf ("Waiting until threads are done\n");
	ITERATE(t,0,THREADS) { //waiting till all of them ends
		  g_thread_join(gth[t]);    }

	if (DEBUG) ITERATE(c,0,7) printf ("    Range stat for range #%1d: %1d\n",c,rangestat[c]);
		  
	//calculating saturation statistics
	if (maindata.preview) {
		//calculating values for infobutton update
		ITERATE(t,0,THREADS) {
			if (finalmaxsat < 	maxsat[t]) finalmaxsat = maxsat[t];
			if (finalminsat > 	minsat[t]) finalminsat = minsat[t];
			finalavgsat=finalavgsat+avgsat[t];}
		finalavgsat=(gfloat) finalavgsat/THREADS;
		//updating infobuttons
		sprintf(labeltextmin, "Lowest saturation:  %.3f",finalminsat);
		gtk_label_set_label(GTK_LABEL(infolabelmin),labeltextmin ); 
		sprintf(labeltextavg, "Average saturation: %.3f",finalavgsat);
		gtk_label_set_label(GTK_LABEL(infolabelavg),labeltextavg ); 
		sprintf(labeltextmax, "Highest saturation: %.3f",finalmaxsat);
		gtk_label_set_label(GTK_LABEL(infolabelmax),labeltextmax ); }
			


	if (maindata.exportlayer) {
		export_to_layer();
		return;}

	//putting data back to drawable
   	if (!maindata.preview)    gimp_pixel_rgn_set_rect (&rgn_out, rect_out_guchar,x1, y1,
    		maindata.width,maindata.height);

  //update (graphical) of preview/image
	if (preview) {
		if (VERBOSE) printf (" Updating preview\n");
		gimp_preview_draw_buffer (preview, rect_out_guchar, maindata.width * maindata.channels);}
    else {
		gimp_drawable_flush (drawable);
		gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
		gimp_drawable_update (drawable->drawable_id,x1,y1,maindata.width,maindata.height);}
		
	g_free (rect_in_guchar);
	g_free (rect_out_guchar);
	
	if (VERBOSE) printf (" =Saturate() ends..\n");
}


static gboolean
sat_dialog (GimpDrawable *drawable)
{
  GtkWidget *dialog;
  GtkWidget *main_hbox,*main_vbox1,*main_vbox2,*hbox_cm;
  GtkWidget *hbox_br,*hbox_se,*ad_hbox,*tmpr_vbox,*skin_hbox,*tmpr_exp,*skin_exp;
  GtkWidget *info_exp, *bal_exp, *info_vbox, *mask_exp;
  GtkWidget *bal_vbox,*bal_hbox,*coup_vbox;

  GtkWidget *label_dark,*label_light,*label_se,*label_br,*label_coup,*bal_label2;

  GtkWidget *label_ad,*label_cm;
  GtkWidget *align_A,*align_B,*align_C,*align_D,*align_E,*align_F,*align_G,*align_H,*align_I;
  GtkWidget *table;

  GtkObject *spin_tl_adj,*spin_tr_adj,*spin_dl_adj,*spin_dr_adj ; 
  
  GtkWidget *exportbutton;

  float slider_h=180;
  float slider_w=8;


  gimp_ui_init ("Saturation Equalizer", FALSE);
  
  gimp_dialogs_show_help_button (FALSE);
  

  dialog = gimp_dialog_new (title, "saturate",
                            NULL, 0,
                            gimp_standard_help_func, "plug-in-saturate",

                            GIMP_STOCK_RESET, RESPONSE_RESET,
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK,     RESPONSE_OK,

                            NULL);

  
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5); // ???

  
  main_hbox = gtk_hbox_new (FALSE, 10);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), main_hbox);
  gtk_widget_show (main_hbox);
  
  
    main_vbox1 = gtk_vbox_new (FALSE, VBOXSPACING);
  gtk_box_pack_start (GTK_BOX (main_hbox), main_vbox1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox1), 5);
  gtk_widget_show (main_vbox1);
   
  main_vbox2 = gtk_vbox_new (FALSE, VBOXSPACING);
  gtk_box_pack_end (GTK_BOX (main_hbox), main_vbox2, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox2), 5);
  gtk_widget_show (main_vbox2);
  

  preview = gimp_zoom_preview_new (drawable);
  gtk_box_pack_start (GTK_BOX (main_vbox1), preview, TRUE, TRUE, 0);
  gtk_widget_show (preview);
  //gtk_widget_set_tooltip_text(preview,labeltextavg);
  
  
 //saturation equalizer label 
  label_se = gtk_label_new ("<b>Saturation Equalizer:</b>");
  gtk_label_set_use_markup (GTK_LABEL(label_se), TRUE);
  gtk_misc_set_alignment (GTK_MISC (label_se), 0.1, 0);
  gtk_box_pack_start (GTK_BOX (main_vbox1), label_se, FALSE, FALSE, 0);
  gtk_widget_show (label_se);
  gtk_widget_set_tooltip_text(label_se, "Sliders works like a saturation curve, \
the amount is a multiplicator of current saturation");

  
  //Saturation equalizer body
  hbox_se = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (main_vbox1),  hbox_se, FALSE, FALSE, 0);
  gtk_widget_show (hbox_se);
  
  	//SLIDERS
	  slider0_adj = gtk_adjustment_new (1.0, 0.0, maxboosts[0], 0.01, 0.1, 0);
	  slider0 = gtk_vscale_new (GTK_ADJUSTMENT (slider0_adj));
	  gtk_scale_set_digits(GTK_SCALE (slider0), 2) ; 
	  gtk_range_set_inverted(GTK_RANGE (slider0),TRUE) ;
	  gtk_widget_set_size_request(slider0,slider_w,slider_h);
	  gtk_box_pack_start (GTK_BOX ( hbox_se), slider0, TRUE, TRUE, 0);
	  gtk_widget_show (slider0);
	 
	  
	  slider1_adj = gtk_adjustment_new (1.0, 0.0, maxboosts[1], 0.01, 0.1, 0);
	  slider1 = gtk_vscale_new (GTK_ADJUSTMENT (slider1_adj));
	  gtk_scale_set_digits(GTK_SCALE (slider1), 2) ; 
	  gtk_range_set_inverted(GTK_RANGE (slider1),TRUE) ;
	  gtk_widget_set_size_request(slider1,slider_w,slider_h);
	  gtk_box_pack_start (GTK_BOX ( hbox_se), slider1, TRUE, TRUE, 0);
	  gtk_widget_show (slider1);
	  
	  slider2_adj = gtk_adjustment_new (1.0, 0.0, maxboosts[2], 0.01, 0.1, 0);
	  slider2 = gtk_vscale_new (GTK_ADJUSTMENT (slider2_adj));
	  gtk_scale_set_digits(GTK_SCALE (slider2), 2) ;  
	  gtk_range_set_inverted(GTK_RANGE (slider2),TRUE) ;
	  gtk_widget_set_size_request(slider2,slider_w,slider_h);
	  gtk_box_pack_start (GTK_BOX ( hbox_se), slider2, TRUE, TRUE, 0);
	  gtk_widget_show (slider2);
	  
	  slider3_adj = gtk_adjustment_new (1.0, 0.0, maxboosts[3], 0.01, 0.1, 0);
	  slider3 = gtk_vscale_new (GTK_ADJUSTMENT (slider3_adj));
	  gtk_scale_set_digits(GTK_SCALE (slider3), 2) ;
	  gtk_range_set_inverted(GTK_RANGE (slider3),TRUE) ;
	  gtk_widget_set_size_request(slider3,slider_w,slider_h);
	  gtk_box_pack_start (GTK_BOX ( hbox_se), slider3, TRUE, TRUE, 0);
	  gtk_widget_show (slider3);  
	  
	  slider4_adj = gtk_adjustment_new (1.0, 0.0, maxboosts[4], 0.01, 0.1, 0);
	  slider4 = gtk_vscale_new (GTK_ADJUSTMENT (slider4_adj));
	  gtk_scale_set_digits(GTK_SCALE (slider4), 2) ;
	  gtk_range_set_inverted(GTK_RANGE (slider4),TRUE) ;  
	  gtk_widget_set_size_request(slider4,slider_w,slider_h);
	  gtk_box_pack_start (GTK_BOX ( hbox_se), slider4, TRUE, TRUE, 0);
	  gtk_widget_show (slider4);

	  slider5_adj = gtk_adjustment_new (1.0, 0.0, maxboosts[5], 0.01, 0.1, 0);
	  slider5 = gtk_vscale_new (GTK_ADJUSTMENT (slider5_adj));
	  gtk_scale_set_digits(GTK_SCALE (slider5), 2) ;
	  gtk_range_set_inverted(GTK_RANGE (slider5),TRUE) ;  
	  gtk_widget_set_size_request(slider5,slider_w,slider_h);
	  gtk_box_pack_start (GTK_BOX ( hbox_se), slider5, TRUE, TRUE, 0);
	  gtk_widget_show (slider5);
  

 
  //BRIGHTNESS
  align_A=gtk_alignment_new(0, 0, 0, 0);
  gtk_box_pack_start (GTK_BOX (main_vbox2), align_A, FALSE, FALSE, 0);
  gtk_alignment_set_padding( GTK_ALIGNMENT( align_A ),3,DOWNINTEND,0,0);
  gtk_widget_show (align_A);

  //brightness label 
  label_br = gtk_label_new ("<b>Brightness adjustment (~levels):</b>");
  gtk_label_set_use_markup (GTK_LABEL(label_br), TRUE);
  gtk_misc_set_alignment (GTK_MISC (label_br), 0.1, 0);
  gtk_container_add(GTK_CONTAINER(align_A),label_br ); 
  gtk_widget_show (label_br);
  gtk_widget_set_tooltip_text(label_br, "Note that RGB values (integers in range 0-255) are \
internally converted to floats in range 0-1");

  
  //Brightness body
  hbox_br = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (main_vbox2),  hbox_br, FALSE, FALSE, 5);
  gtk_container_set_border_width (GTK_CONTAINER (hbox_br), INNERBOXBORDER);
  gtk_widget_show (hbox_br);
  
 	  //set label for dark
	  label_dark = gtk_label_new("Dark: ");
	  gtk_box_pack_start (GTK_BOX ( hbox_br), label_dark, TRUE, FALSE, 0);
	  gtk_widget_show (label_dark);
	   
	  spin_dark_adj = gtk_adjustment_new (0, -0.4,DARKUPLIMIT, 0.01, 0.1, 0);
	  spin_dark = gtk_spin_button_new (GTK_ADJUSTMENT (spin_dark_adj), 0.1, 3);
	  gtk_widget_show (spin_dark);
	  gtk_box_pack_start (GTK_BOX (hbox_br), spin_dark, TRUE, FALSE,0);
	  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin_dark), TRUE);
	  gtk_widget_show (spin_dark);
	  gtk_widget_set_tooltip_text(spin_dark, "Set new brightness for black pixels");
	 
	  //set label for light
	  label_light = gtk_label_new("  Light: ");
	  gtk_box_pack_start (GTK_BOX (hbox_br), label_light, TRUE, FALSE, 0);
	  gtk_widget_show (label_light);
	  
	  spin_light_adj = gtk_adjustment_new (1, LIGHTDOWNLIMIT, 1.4, 0.01, 0.1, 0);
	  spin_light = gtk_spin_button_new (GTK_ADJUSTMENT (spin_light_adj), 0.1, 3);
	  gtk_widget_show (spin_light);
	  gtk_box_pack_start (GTK_BOX (hbox_br), spin_light, TRUE, FALSE, 0);
	  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin_light), TRUE);
	  gtk_widget_show (spin_light);
	  gtk_widget_set_tooltip_text(spin_light, "Set new brightness for white pixels");

    
  //SLIDERS COUPLING
   align_C=gtk_alignment_new(0, 0, 0, 0);
  gtk_box_pack_start (GTK_BOX (main_vbox2), align_C, FALSE, FALSE, 0);
  gtk_alignment_set_padding( GTK_ALIGNMENT( align_C ),UPINTEND,DOWNINTEND,0,0);
  gtk_widget_show (align_C);
	
  label_coup = gtk_label_new ("<b>Sliders coupling:</b>");
  gtk_label_set_use_markup (GTK_LABEL(label_coup), TRUE);
  gtk_misc_set_alignment (GTK_MISC (label_coup), 0.1, 0);
  gtk_container_add(GTK_CONTAINER(align_C),label_coup ); 
  gtk_widget_show (label_coup);
  
  //vbox for coupling
  coup_vbox = gtk_vbox_new (FALSE,0);
  gtk_box_pack_start (GTK_BOX (main_vbox2),  coup_vbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (coup_vbox), INNERBOXBORDER);
  gtk_widget_show(coup_vbox);
  
  //checouplebutton to activate coupling
  couplebutton= gtk_check_button_new_with_label("Sliders coupled");
  gtk_box_pack_start (GTK_BOX (coup_vbox), couplebutton, FALSE, FALSE, 0);
  gtk_widget_set_size_request(couplebutton,0,40);
  gtk_widget_show (couplebutton);
  gtk_widget_set_tooltip_text( couplebutton, "Make other sliders folow the one you draw");
  
  
  //VBOX for spinner
  ad_hbox = gtk_hbox_new (FALSE,10);
  gtk_box_pack_start (GTK_BOX (coup_vbox),  ad_hbox, FALSE, FALSE, 0);
  //gtk_container_set_border_width (GTK_CONTAINER (ad_hbox), INNERBOXBORDER);
  gtk_widget_show(ad_hbox);
  
	label_ad = gtk_label_new ("Allowed difference:");
	gtk_box_pack_start (GTK_BOX (ad_hbox),  label_ad, FALSE, FALSE, 0);
	gtk_widget_show (label_ad);
	
	spin_maxdiff_adj = gtk_adjustment_new (0.6, 0,8, 0.01, 0.1, 0);
	spin_maxdiff = gtk_spin_button_new (GTK_ADJUSTMENT (spin_maxdiff_adj), 0.1, 2);
	gtk_widget_show (spin_maxdiff);
	gtk_widget_set_sensitive(spin_maxdiff,FALSE);
	gtk_box_pack_start (GTK_BOX (ad_hbox), spin_maxdiff, FALSE, FALSE, 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin_maxdiff), TRUE);
	gtk_widget_show (spin_maxdiff);

  //color mode
  align_D=gtk_alignment_new(0, 0, 0, 0);
  gtk_box_pack_start (GTK_BOX (main_vbox2), align_D, FALSE, FALSE, 0);
  gtk_alignment_set_padding( GTK_ALIGNMENT( align_D ),UPINTEND,DOWNINTEND,0,0);
  gtk_widget_show (align_D);

	
  //Color Model hbox
  hbox_cm = gtk_hbox_new (FALSE, 0);
  gtk_container_add(GTK_CONTAINER(align_D),hbox_cm ); 
  gtk_widget_show (hbox_cm);

  //color mode label 
  label_cm = gtk_label_new ("<b>Color Mode:    </b>");
  gtk_label_set_use_markup (GTK_LABEL(label_cm), TRUE);
  gtk_misc_set_alignment (GTK_MISC (label_cm), 0.1, 0.5);
  gtk_box_pack_start (GTK_BOX (hbox_cm), label_cm, TRUE, FALSE, 0);
  gtk_widget_show (label_cm);

  combo = gtk_combo_box_new_text();
  gtk_combo_box_append_text( GTK_COMBO_BOX( combo),mode2);
  gtk_combo_box_append_text( GTK_COMBO_BOX( combo),mode6);
  gtk_combo_box_append_text( GTK_COMBO_BOX( combo),mode3);
  gtk_combo_box_append_text( GTK_COMBO_BOX( combo),mode4); 
  gtk_combo_box_append_text( GTK_COMBO_BOX( combo),mode5); 
  gtk_box_pack_end (GTK_BOX (hbox_cm), combo, FALSE, FALSE, 0);
  gtk_combo_box_set_active(GTK_COMBO_BOX( combo), FORMULADEFAULT);
  gtk_widget_show (combo);


   //QUICK SUN FILTERS
  align_E=gtk_alignment_new(0, 0, 0, 0);
  gtk_box_pack_start (GTK_BOX (main_vbox2), align_E, FALSE, FALSE, 0);
  gtk_alignment_set_padding( GTK_ALIGNMENT( align_E ),UPINTEND,DOWNINTEND,0,0);
  gtk_widget_show (align_E);

  mask_exp = gtk_expander_new  ("<b>Per-Corner Brightness:</b>" );
  gtk_expander_set_use_markup (GTK_EXPANDER(mask_exp), TRUE);
  gtk_container_add(GTK_CONTAINER(align_E),mask_exp ); 
  gtk_widget_set_tooltip_text( mask_exp, "4 spinboxes corresponding to 4 corners to finetune the brightness of image.");
  gtk_widget_show (mask_exp);

  // ############################   
  	table=gtk_table_new(2,2,FALSE);
	gtk_container_add(GTK_CONTAINER(mask_exp), table);
	gtk_container_set_border_width (GTK_CONTAINER (table), 12);
	gtk_table_set_col_spacings(GTK_TABLE(table),20);
	gtk_table_set_row_spacings(GTK_TABLE(table),5);
	gtk_widget_show (table); 	
	   
	   	spin_tl_adj = gtk_adjustment_new (0, -0.2,0.2, 0.01,0.01, 0);
		spin_tl = gtk_spin_button_new (GTK_ADJUSTMENT (spin_tl_adj), 0.01, 3);
		gtk_table_attach_defaults (GTK_TABLE(table),spin_tl,0,1,0,1);
		gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin_tl), TRUE);
		gtk_widget_set_tooltip_text( spin_tl, "Adjust upper left corner of image.");
		gtk_widget_show (spin_tl);
 
 	   	spin_tr_adj = gtk_adjustment_new (0, -0.2,0.2, 0.01,0.01, 0);
		spin_tr = gtk_spin_button_new (GTK_ADJUSTMENT (spin_tr_adj), 0.01, 3);
		gtk_table_attach_defaults (GTK_TABLE(table),spin_tr,1,2,0,1);
		gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin_tr), TRUE);
		gtk_widget_set_tooltip_text( spin_tr, "Adjust upper right corner of image.");
		gtk_widget_show (spin_tr);
 
 	   	spin_dl_adj = gtk_adjustment_new (0, -0.2,0.2, 0.01,0.01, 0);
		spin_dl = gtk_spin_button_new (GTK_ADJUSTMENT (spin_dl_adj), 0.01, 3);
		gtk_table_attach_defaults (GTK_TABLE(table),spin_dl,0,1,1,2);
		gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin_dl), TRUE);
		gtk_widget_set_tooltip_text( spin_dl, "Adjust bottom left corner of image.");		
		gtk_widget_show (spin_dl);
 
 	   	spin_dr_adj = gtk_adjustment_new (0, -0.2,0.2, 0.01,0.01, 0);
		spin_dr = gtk_spin_button_new (GTK_ADJUSTMENT (spin_dr_adj), 0.01, 3);
		gtk_table_attach_defaults (GTK_TABLE(table),spin_dr,1,2,1,2);
		gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin_dr), TRUE);
		gtk_widget_set_tooltip_text( spin_dr, "Adjust bottom right corner of image.");	
		gtk_widget_show (spin_dr);
 
  //TEMPERATURE SECTION
  align_B=gtk_alignment_new(0, 0, 0, 0);
  gtk_box_pack_start (GTK_BOX (main_vbox2), align_B, FALSE, FALSE, 0);
  gtk_alignment_set_padding( GTK_ALIGNMENT( align_B ),0,2,0,0);
  gtk_widget_show (align_B);

  tmpr_exp = gtk_expander_new  ("<b>Color Temperature modification:</b>" );
  gtk_expander_set_use_markup (GTK_EXPANDER(tmpr_exp), TRUE);
  gtk_container_add(GTK_CONTAINER(align_B),tmpr_exp ); 
  gtk_widget_show (tmpr_exp);

	tmpr_vbox= gtk_vbox_new (FALSE,5);
	gtk_container_set_border_width (GTK_CONTAINER (tmpr_vbox), INNERBOXBORDER);
	gtk_container_add (GTK_CONTAINER (tmpr_exp), tmpr_vbox);
	
	gtk_widget_show(tmpr_vbox);

  	  slider_tmpr_adj = gtk_adjustment_new (0.0, -1, 1, 0.01, 0.02, 0);
	  slider_tmpr = gtk_hscale_new (GTK_ADJUSTMENT (slider_tmpr_adj));
	  gtk_scale_set_digits(GTK_SCALE (slider_tmpr), 2) ;
	  gtk_box_pack_start (GTK_BOX ( tmpr_vbox), slider_tmpr, TRUE, TRUE, 0);
	  gtk_scale_set_value_pos (GTK_SCALE (slider_tmpr),GTK_POS_RIGHT);	
	  gtk_widget_show (slider_tmpr);
	  gtk_widget_set_tooltip_text(slider_tmpr, "Move rightward for wormer and leftward for colder temperature");

  //COLOR BALANCE SECTION
  align_H=gtk_alignment_new(0, 0, 0, 0);
  gtk_box_pack_start (GTK_BOX (main_vbox2), align_H, FALSE, FALSE, 0);
  gtk_alignment_set_padding( GTK_ALIGNMENT( align_H ),0,2,0,0);
  gtk_widget_show (align_H);

  bal_exp = gtk_expander_new  ("<b>Color Auto Balance:</b>" );
  gtk_expander_set_use_markup (GTK_EXPANDER(bal_exp), TRUE);
  gtk_container_add(GTK_CONTAINER(align_H),bal_exp ); 
  gtk_widget_set_tooltip_text(bal_exp, "Automatic white balancing, usefull\
 to get rid of color cast (wrong temperature, colored snow and so) or to make whole image more colorfull.");
  gtk_widget_show (bal_exp);

	bal_vbox= gtk_vbox_new (FALSE,8);
	gtk_container_add (GTK_CONTAINER (bal_exp), bal_vbox);
	gtk_container_set_border_width (GTK_CONTAINER (bal_vbox), INNERBOXBORDER);
	gtk_widget_show(bal_vbox);

		bal_button = gtk_button_new_with_label("Calculate and Apply");
		gtk_button_set_alignment(GTK_BUTTON(bal_button),0.5,0.5);
		gtk_box_pack_start (GTK_BOX (bal_vbox), bal_button, FALSE, FALSE, 0);
 		gtk_widget_set_tooltip_text(bal_button, "Zoom preview to an area to be white balanced and click here, \
calculated adjustmens are remembered and applied to whole image.");
		gtk_widget_show (bal_button);

		bal_label=gtk_label_new(shiftdef);
		gtk_box_pack_start (GTK_BOX (bal_vbox), bal_label, FALSE, FALSE, 0);
		gtk_misc_set_alignment (GTK_MISC (bal_label), 0, 0.5);
		gtk_widget_show (bal_label); 
		gtk_widget_set_tooltip_text( bal_label, "Calculated values, the final/applied amount can be reduced with slider below");

		
		bal_hbox= gtk_hbox_new (FALSE,0);
		gtk_box_pack_start (GTK_BOX (bal_vbox), bal_hbox, FALSE, FALSE, 0);
		gtk_widget_show(bal_hbox);
		
			bal_label2=gtk_label_new("Weight: ");
			gtk_box_pack_start (GTK_BOX (bal_hbox), bal_label2, FALSE, FALSE, 0);
			gtk_misc_set_alignment (GTK_MISC (bal_label2), 0, 0.5);
			gtk_widget_show (bal_label2);		
			gtk_widget_set_tooltip_text( bal_label2, "Weight of effect, zero position = functionality is disabled");

	  
			slider_bal_adj = gtk_adjustment_new (1, 0, 1, 0.01, 0.02, 0);
			slider_bal = gtk_hscale_new (GTK_ADJUSTMENT (slider_bal_adj));
			gtk_scale_set_digits(GTK_SCALE (slider_bal), 2) ;
			gtk_widget_set_size_request(slider_bal,150,30);
			gtk_box_pack_start (GTK_BOX ( bal_hbox), slider_bal, TRUE, TRUE, 0);
			gtk_scale_set_value_pos (GTK_SCALE (slider_bal),GTK_POS_RIGHT);	
			gtk_widget_show (slider_bal);
			gtk_widget_set_tooltip_text( slider_bal, "Weight of effect, zero position = functionality is disabled");

 

	//SKIN PROTECTION SECTION
  align_F=gtk_alignment_new(0, 0, 0, 0);
  gtk_box_pack_start (GTK_BOX (main_vbox2), align_F, FALSE, FALSE, 0);
  gtk_alignment_set_padding( GTK_ALIGNMENT( align_F ),0,2,0,0);
  gtk_widget_show (align_F);


  skin_exp = gtk_expander_new  ("<b>Limit saturation to color:</b>" );
  gtk_expander_set_use_markup (GTK_EXPANDER(skin_exp), TRUE);
  gtk_container_add(GTK_CONTAINER(align_F),skin_exp ); 
  gtk_widget_show (skin_exp);
  gtk_widget_set_tooltip_text( skin_exp, "Restrict the saturation to selected color\
 (Inverted selection/mode also available)");

	skin_hbox= gtk_hbox_new (FALSE,5);
	gtk_container_add (GTK_CONTAINER (skin_exp), skin_hbox);
	gtk_container_set_border_width (GTK_CONTAINER (skin_hbox), INNERBOXBORDER);
	gtk_widget_show(skin_hbox);
	
  	  slider_skin_adj = gtk_adjustment_new (0.0, -0.08, 0.08, 0.002, 0.002, 0);
	  slider_skin = gtk_hscale_new (GTK_ADJUSTMENT (slider_skin_adj));
	  gtk_scale_set_digits(GTK_SCALE (slider_skin), 3) ;
	  gtk_widget_set_size_request(slider_skin,150,30);
	  gtk_box_pack_start (GTK_BOX ( skin_hbox), slider_skin, TRUE, TRUE, 0);
	  gtk_widget_show (slider_skin);
	  gtk_widget_set_tooltip_text(slider_skin, "Defines range of affected colors around picked color.\
 Portion to the left is for 'inverted selection'.");
	  
	  skinselector= gtk_color_button_new_with_color(&skincolor);
	  gtk_box_pack_end (GTK_BOX ( skin_hbox), skinselector, FALSE, FALSE, 0);
	  gtk_widget_show (skinselector);
	  gtk_widget_set_tooltip_text(skinselector, "Choose different color, you can use the eyedropper. NOTE: Brightness of selected color is ignored.");
 

  //INFO BOX SECTION
  align_G=gtk_alignment_new(0, 0, 0, 0);
  gtk_box_pack_start (GTK_BOX (main_vbox2), align_G, FALSE, FALSE, 0);
  gtk_alignment_set_padding( GTK_ALIGNMENT( align_G ),0,2,0,0);
  gtk_widget_show (align_G);

  info_exp = gtk_expander_new  ("<b>Saturation statistics (in preview):</b>" );
  gtk_expander_set_use_markup (GTK_EXPANDER(info_exp), TRUE);
  gtk_container_add(GTK_CONTAINER(align_G),info_exp ); 
  gtk_widget_show (info_exp);

	info_vbox= gtk_vbox_new (FALSE,5);
	gtk_container_add (GTK_CONTAINER (info_exp), info_vbox);
	gtk_container_set_border_width (GTK_CONTAINER (info_vbox), INNERBOXBORDER);
	gtk_widget_show(info_vbox);

	//infolabels for saturation iformation
	infolabelmin=gtk_label_new("");
	gtk_box_pack_start (GTK_BOX (info_vbox), infolabelmin, FALSE, FALSE, 0);
	gtk_misc_set_alignment (GTK_MISC (infolabelmin), 0, 0.5);
	//gtk_widget_set_size_request(infolabelmin,300,20);
	gtk_widget_show (infolabelmin); 
	infolabelavg=gtk_label_new("");
	gtk_box_pack_start (GTK_BOX (info_vbox), infolabelavg, FALSE, FALSE, 0);
	gtk_misc_set_alignment (GTK_MISC (infolabelavg), 0, 0.5);
	gtk_widget_show (infolabelavg); 
	infolabelmax=gtk_label_new("");
	gtk_box_pack_start (GTK_BOX (info_vbox), infolabelmax, FALSE, FALSE, 0);
	gtk_misc_set_alignment (GTK_MISC (infolabelmax), 0, 0.5);
	gtk_widget_show (infolabelmax); 

  align_I=gtk_alignment_new(0.5, 0, 0, 0);
  gtk_box_pack_end (GTK_BOX (main_vbox2), align_I, FALSE, FALSE, 0);
  gtk_alignment_set_padding( GTK_ALIGNMENT( align_I ),UPINTEND,DOWNINTEND,0,0);
  gtk_widget_show (align_I);
  
  	  exportbutton = gtk_button_new_with_label("Export as new layer");
  	  gtk_widget_set_tooltip_text(exportbutton, "Plugin window will stay alive with all settings preserved");
	  gtk_widget_set_size_request(exportbutton,170,35);
	  gtk_button_set_alignment(GTK_BUTTON(exportbutton),0.5,0.5);
	  gtk_container_add(GTK_CONTAINER(align_I),exportbutton );
	  gtk_widget_show (exportbutton);

	//these are GUI actions that trigger preview update, other actions triger preview update
	//from within own functions
	g_signal_connect_swapped (preview, "invalidated",  G_CALLBACK (saturate),drawable);
	g_signal_connect_swapped (slider0_adj    , "value_changed", G_CALLBACK (gimp_preview_invalidate), preview);
	g_signal_connect_swapped (slider1_adj    , "value_changed", G_CALLBACK (gimp_preview_invalidate), preview);
	g_signal_connect_swapped (slider2_adj    , "value_changed", G_CALLBACK (gimp_preview_invalidate), preview);
	g_signal_connect_swapped (slider3_adj    , "value_changed", G_CALLBACK (gimp_preview_invalidate), preview);
	g_signal_connect_swapped (slider4_adj    , "value_changed", G_CALLBACK (gimp_preview_invalidate), preview);
	g_signal_connect_swapped (slider5_adj    , "value_changed", G_CALLBACK (gimp_preview_invalidate), preview);
	g_signal_connect_swapped (spin_dark_adj  , "value_changed", G_CALLBACK (gimp_preview_invalidate), preview);
	g_signal_connect_swapped (spin_light_adj , "value_changed", G_CALLBACK (gimp_preview_invalidate), preview);
	g_signal_connect_swapped (slider_tmpr_adj, "value_changed", G_CALLBACK (gimp_preview_invalidate), preview);
	g_signal_connect_swapped (slider_skin_adj, "value_changed", G_CALLBACK (gimp_preview_invalidate), preview);
	g_signal_connect_swapped (slider_bal_adj , "value_changed", G_CALLBACK (gimp_preview_invalidate), preview);
	g_signal_connect_swapped (spin_tl_adj    , "value_changed", G_CALLBACK (gimp_preview_invalidate),preview);
	g_signal_connect_swapped (spin_tr_adj    , "value_changed", G_CALLBACK (gimp_preview_invalidate),preview);
	g_signal_connect_swapped (spin_dl_adj    , "value_changed", G_CALLBACK (gimp_preview_invalidate),preview);
	g_signal_connect_swapped (spin_dr_adj    , "value_changed", G_CALLBACK (gimp_preview_invalidate),preview);
	g_signal_connect_swapped (G_OBJECT(skinselector   ),"color-set", G_CALLBACK (gimp_preview_invalidate),preview);	
	g_signal_connect_swapped (G_OBJECT(combo          ), "changed" , G_CALLBACK (gimp_preview_invalidate),preview);
	g_signal_connect_swapped (G_OBJECT(bal_button     ), "clicked" , G_CALLBACK (gimp_preview_invalidate),preview);


 
 saturate (drawable, GIMP_PREVIEW (preview));

	//simple update of values based on user (GUI) actions
	g_signal_connect (spin_dark_adj  ,"value_changed",G_CALLBACK (gimp_float_adjustment_update),&maindata.levels[0]);  
	g_signal_connect (spin_light_adj ,"value_changed",G_CALLBACK (gimp_float_adjustment_update),&maindata.levels[1]);
	g_signal_connect (slider_tmpr_adj,"value_changed",G_CALLBACK (gimp_float_adjustment_update),&maindata.temperature);
	g_signal_connect (slider_skin_adj,"value_changed",G_CALLBACK (gimp_float_adjustment_update),&maindata.skin);
	g_signal_connect (slider_bal_adj ,"value_changed",G_CALLBACK (gimp_float_adjustment_update),&maindata.balweight);
	g_signal_connect (spin_tl_adj , "value_changed", G_CALLBACK (gimp_float_adjustment_update), &maindata.cornerbr[0]); 
	g_signal_connect (spin_tr_adj , "value_changed", G_CALLBACK (gimp_float_adjustment_update), &maindata.cornerbr[1]); 
	g_signal_connect (spin_dl_adj , "value_changed", G_CALLBACK (gimp_float_adjustment_update), &maindata.cornerbr[2]); 
	g_signal_connect (spin_dr_adj , "value_changed", G_CALLBACK (gimp_float_adjustment_update), &maindata.cornerbr[3]); 
	
	//some actions initiate functions that update values and do more 
	g_signal_connect (slider0_adj, "value_changed", G_CALLBACK (slider_update), &sliders[0]);
	g_signal_connect (slider1_adj, "value_changed", G_CALLBACK (slider_update), &sliders[1]);
	g_signal_connect (slider2_adj, "value_changed", G_CALLBACK (slider_update), &sliders[2]);
	g_signal_connect (slider3_adj, "value_changed", G_CALLBACK (slider_update), &sliders[3]);
	g_signal_connect (slider4_adj, "value_changed", G_CALLBACK (slider_update), &sliders[4]);
	g_signal_connect (slider5_adj, "value_changed", G_CALLBACK (slider_update), &sliders[5]);

	g_signal_connect( G_OBJECT( combo           ), "changed",G_CALLBACK( alg_changed      ), NULL );
	g_signal_connect( G_OBJECT( couplebutton    ), "toggled",G_CALLBACK( coupled_changed  ), NULL );
	g_signal_connect( G_OBJECT( bal_button      ), "clicked",G_CALLBACK( calibrate        ), NULL );

	g_signal_connect_swapped( G_OBJECT(exportbutton),"clicked",G_CALLBACK( exportwrapper ), drawable );

	gtk_widget_show (dialog);

	//receiving signals from gimp buttons + "x" (close window button)
	g_signal_connect (dialog, "response",
					G_CALLBACK (response_callback),
					NULL);

  gtk_main ();

  return process_image; //TRUE if image should be processed

}

static void response_callback (GtkWidget *widget,  gint response_id) {

	
  switch (response_id) 	 {
    case RESPONSE_RESET:
      if (VERBOSE) printf ("Plugin window returned: RESET... \n");
		gtk_range_set_value (GTK_RANGE(slider0),1);
		gtk_range_set_value (GTK_RANGE(slider1),1);
		gtk_range_set_value (GTK_RANGE(slider2),1);
		gtk_range_set_value (GTK_RANGE(slider3),1);
		gtk_range_set_value (GTK_RANGE(slider4),1);
		gtk_range_set_value (GTK_RANGE(slider5),1);
		 
		 gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_dark ),0);
		 gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_light),1);
		 
		 gtk_spin_button_set_value (GTK_SPIN_BUTTON(spin_tl),0);  
		 gtk_spin_button_set_value (GTK_SPIN_BUTTON(spin_tr),0); 
		 gtk_spin_button_set_value (GTK_SPIN_BUTTON(spin_dl),0); 
		 gtk_spin_button_set_value (GTK_SPIN_BUTTON(spin_dr),0); 
		 
		 gtk_range_set_value (GTK_RANGE(slider_tmpr),0);   
		 gtk_range_set_value (GTK_RANGE(slider_skin),0);  
		 gtk_range_set_value (GTK_RANGE(slider_bal),COLWEIGHTDEF);  

		 maindata.formula=FORMULADEFAULT;
		 gtk_combo_box_set_active(GTK_COMBO_BOX( combo), FORMULADEFAULT);

		 gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (couplebutton   ),FALSE);

		gtk_widget_set_sensitive(spin_maxdiff,FALSE);

		 skincolor.red=SKINSELECTORR;
		 skincolor.green=SKINSELECTORG;
		 skincolor.blue=SKINSELECTORB;
		 gtk_color_button_set_color   ((GtkColorButton*) skinselector, &skincolor);
		 
		maindata.colorbal[0]=0;
		maindata.colorbal[1]=0;
		maindata.colorbal[2]=0;
		if (maindata.docolbal) {
			maindata.docolbal =FALSE;
			gimp_preview_invalidate(GIMP_PREVIEW(preview));}
		gtk_label_set_label(GTK_LABEL(bal_label),shiftdef );
		 
		 break;

    case RESPONSE_OK:  //quitting plugin window and applying change
      if (VERBOSE) printf ("Plugin window returned: OK... \n");
		process_image=TRUE; 
		gtk_widget_destroy (widget);
		gtk_main_quit ();   
 		break;

    default: // if other response - terminate plugin windo
      if (VERBOSE) printf ("Plugin window quitted... \n");
		gtk_widget_destroy (widget);
		gtk_main_quit ();
		break;
    };
	
}

