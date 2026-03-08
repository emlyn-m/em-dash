/*
* 	layout (layer 0)
*        q w e r t y u i o p
*         a s d f g h j k l
*        sh z x c v b n m \b
*       123 [    sp    ]. \n
*
*	shift (layer 1)
*        Q W E R T Y U I O P
*         A S D F G H J K L
*        sh Z X C V B N M \b
*       123 [    sp    ]. \n
*
*	123 layer (layer 2)
*	    1 2 3 4 5 6 7 8 9 0
*		 £ & ( ) " ' - + /
*	   #~= @ ! ? : ; , … \b
*	   abc [    sp    ]. \n
*
*	#~= layer (layer 3)
*	    # % ~ ^ [ ] { } | \
*		 . ™ ¬ ` < > _ * =
*	   123 © ® § ¢ ¥ € $ \b
*	   abc [    sp    ]. \n
*/


#define KB_LAYER_NORM 0
#define KB_LAYER_SHFT 1
#define KB_LAYER_CAPS 2
#define KB_LAYER_NUMS 3
#define KB_LAYER_SPCL 4

#define GOTO_KB_NORM KB_LAYER_NORM+1
#define GOTO_KB_SHFT KB_LAYER_SHFT+1
#define GOTO_KB_CAPS KB_LAYER_CAPS+1
#define GOTO_KB_NUMS KB_LAYER_NUMS+1
#define GOTO_KB_SPCL KB_LAYER_SPCL+1

// Kindle PW3 touchscreen EV_ABS axis codes (not in SDK headers)
#define KINDLE_ABS_X 53
#define KINDLE_ABS_Y 54
