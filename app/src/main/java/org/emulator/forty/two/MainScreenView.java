package org.emulator.forty.two;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;

import android.graphics.Paint;
import android.graphics.RectF;
import android.os.Build;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceView;

import java.util.HashMap;

public class MainScreenView extends PanAndScaleView {

    protected static final String TAG = "MainScreenView";
    private Paint paint = new Paint();
    private Bitmap bitmapMainScreen;
    private HashMap<Integer, Integer> vkmap;
//    private float screenScaleX = 1.0f;
//    private float screenScaleY = 1.0f;
//    private float screenOffsetX = 0.0f;
//    private float screenOffsetY= 0.0f;
//    private boolean fillScreen = false;
    private float fixScale = 0.0f;
    private boolean horizontalSwipe = true;
    private int kmlBackgroundColor = Color.BLACK;
    private boolean useKmlBackgroundColor = false;
    private int fallbackBackgroundColorType = 0;
    private int statusBarColor = 0;
    private boolean viewSized = false;

    public MainScreenView(Context context) {
        super(context);

        setShowScaleThumbnail(true);
        setAllowDoubleTapZoom(false);

        paint.setFilterBitmap(true);
        paint.setAntiAlias(true);

        DisplayMetrics displayMetrics = new DisplayMetrics();
        ((Activity)context).getWindowManager().getDefaultDisplay().getMetrics(displayMetrics);
        bitmapMainScreen = Bitmap.createBitmap(displayMetrics.widthPixels, displayMetrics.heightPixels, Bitmap.Config.ARGB_8888);
        bitmapMainScreen.eraseColor(Color.BLACK);

        vkmap = new HashMap<>();
        //vkmap.put(KeyEvent.KEYCODE_BACK, 0x08); // VK_BACK
        vkmap.put(KeyEvent.KEYCODE_TAB, 0x09); // VK_TAB
        vkmap.put(KeyEvent.KEYCODE_ENTER, 0x0D); // VK_RETURN
        vkmap.put(KeyEvent.KEYCODE_NUMPAD_ENTER, 0x0D); // VK_RETURN
        //vkmap.put(KeyEvent.KEYCODE_DEL, 0x2E); // VK_DELETE
        vkmap.put(KeyEvent.KEYCODE_DEL, 0x08); // VK_DELETE
        vkmap.put(KeyEvent.KEYCODE_INSERT, 0x2D); // VK_INSERT
        vkmap.put(KeyEvent.KEYCODE_SHIFT_LEFT, 0x10); // VK_SHIFT
        vkmap.put(KeyEvent.KEYCODE_SHIFT_RIGHT, 0x10); // VK_SHIFT
        vkmap.put(KeyEvent.KEYCODE_CTRL_LEFT, 0x11); // VK_CONTROL
        vkmap.put(KeyEvent.KEYCODE_CTRL_RIGHT, 0x11); // VK_CONTROL
        vkmap.put(KeyEvent.KEYCODE_ESCAPE, 0x1B); // VK_ESCAPE
        vkmap.put(KeyEvent.KEYCODE_SPACE, 0x20); // VK_SPACE
        vkmap.put(KeyEvent.KEYCODE_DPAD_LEFT, 0x25); // VK_LEFT
        vkmap.put(KeyEvent.KEYCODE_DPAD_UP, 0x26); // VK_UP
        vkmap.put(KeyEvent.KEYCODE_DPAD_RIGHT, 0x27); // VK_RIGHT
        vkmap.put(KeyEvent.KEYCODE_DPAD_DOWN, 0x28); // VK_DOWN
//        vkmap.put(KeyEvent.KEYCODE_0, 0x30); // 0
//        vkmap.put(KeyEvent.KEYCODE_1, 0x31); // 1
//        vkmap.put(KeyEvent.KEYCODE_2, 0x32); // 2
//        vkmap.put(KeyEvent.KEYCODE_3, 0x33); // 3
//        vkmap.put(KeyEvent.KEYCODE_4, 0x34); // 4
//        vkmap.put(KeyEvent.KEYCODE_5, 0x35); // 5
//        vkmap.put(KeyEvent.KEYCODE_6, 0x36); // 6
//        vkmap.put(KeyEvent.KEYCODE_7, 0x37); // 7
//        vkmap.put(KeyEvent.KEYCODE_8, 0x38); // 8
//        vkmap.put(KeyEvent.KEYCODE_9, 0x39); // 9
        vkmap.put(KeyEvent.KEYCODE_A, 0x41); // A
        vkmap.put(KeyEvent.KEYCODE_B, 0x42); // B
        vkmap.put(KeyEvent.KEYCODE_C, 0x43); // C
        vkmap.put(KeyEvent.KEYCODE_D, 0x44); // D
        vkmap.put(KeyEvent.KEYCODE_E, 0x45); // E
        vkmap.put(KeyEvent.KEYCODE_F, 0x46); // F
        vkmap.put(KeyEvent.KEYCODE_G, 0x47); // G
        vkmap.put(KeyEvent.KEYCODE_H, 0x48); // H
        vkmap.put(KeyEvent.KEYCODE_I, 0x49); // I
        vkmap.put(KeyEvent.KEYCODE_J, 0x4A); // J
        vkmap.put(KeyEvent.KEYCODE_K, 0x4B); // K
        vkmap.put(KeyEvent.KEYCODE_L, 0x4C); // L
        vkmap.put(KeyEvent.KEYCODE_M, 0x4D); // M
        vkmap.put(KeyEvent.KEYCODE_N, 0x4E); // N
        vkmap.put(KeyEvent.KEYCODE_O, 0x4F); // O
        vkmap.put(KeyEvent.KEYCODE_P, 0x50); // P
        vkmap.put(KeyEvent.KEYCODE_Q, 0x51); // Q
        vkmap.put(KeyEvent.KEYCODE_R, 0x52); // R
        vkmap.put(KeyEvent.KEYCODE_S, 0x53); // S
        vkmap.put(KeyEvent.KEYCODE_T, 0x54); // T
        vkmap.put(KeyEvent.KEYCODE_U, 0x55); // U
        vkmap.put(KeyEvent.KEYCODE_V, 0x56); // V
        vkmap.put(KeyEvent.KEYCODE_W, 0x57); // W
        vkmap.put(KeyEvent.KEYCODE_X, 0x58); // X
        vkmap.put(KeyEvent.KEYCODE_Y, 0x59); // Y
        vkmap.put(KeyEvent.KEYCODE_Z, 0x5A); // Z
        vkmap.put(KeyEvent.KEYCODE_NUMPAD_0, 0x60); // VK_NUMPAD0
        vkmap.put(KeyEvent.KEYCODE_NUMPAD_1, 0x61); // VK_NUMPAD1
        vkmap.put(KeyEvent.KEYCODE_NUMPAD_2, 0x62); // VK_NUMPAD2
        vkmap.put(KeyEvent.KEYCODE_NUMPAD_3, 0x63); // VK_NUMPAD3
        vkmap.put(KeyEvent.KEYCODE_NUMPAD_4, 0x64); // VK_NUMPAD4
        vkmap.put(KeyEvent.KEYCODE_NUMPAD_5, 0x65); // VK_NUMPAD5
        vkmap.put(KeyEvent.KEYCODE_NUMPAD_6, 0x66); // VK_NUMPAD6
        vkmap.put(KeyEvent.KEYCODE_NUMPAD_7, 0x67); // VK_NUMPAD7
        vkmap.put(KeyEvent.KEYCODE_NUMPAD_8, 0x68); // VK_NUMPAD8
        vkmap.put(KeyEvent.KEYCODE_NUMPAD_9, 0x69); // VK_NUMPAD9
        vkmap.put(KeyEvent.KEYCODE_0, 0x60); // VK_NUMPAD0
        vkmap.put(KeyEvent.KEYCODE_1, 0x61); // VK_NUMPAD1
        vkmap.put(KeyEvent.KEYCODE_2, 0x62); // VK_NUMPAD2
        vkmap.put(KeyEvent.KEYCODE_3, 0x63); // VK_NUMPAD3
        vkmap.put(KeyEvent.KEYCODE_4, 0x64); // VK_NUMPAD4
        vkmap.put(KeyEvent.KEYCODE_5, 0x65); // VK_NUMPAD5
        vkmap.put(KeyEvent.KEYCODE_6, 0x66); // VK_NUMPAD6
        vkmap.put(KeyEvent.KEYCODE_7, 0x67); // VK_NUMPAD7
        vkmap.put(KeyEvent.KEYCODE_8, 0x68); // VK_NUMPAD8
        vkmap.put(KeyEvent.KEYCODE_9, 0x69); // VK_NUMPAD9
        vkmap.put(KeyEvent.KEYCODE_NUMPAD_MULTIPLY, 0x6A); // VK_MULTIPLY
        vkmap.put(KeyEvent.KEYCODE_NUMPAD_ADD, 0x6B); // VK_ADD
        vkmap.put(KeyEvent.KEYCODE_NUMPAD_SUBTRACT, 0x6D); // VK_SUBTRACT
        vkmap.put(KeyEvent.KEYCODE_NUMPAD_DOT, 0x6E); // VK_DECIMAL
        vkmap.put(KeyEvent.KEYCODE_NUMPAD_DIVIDE, 0x6F); // VK_DIVIDE
//        vkmap.put(KeyEvent.KEYCODE_SEMICOLON, 0xBA); // VK_OEM_1 (:);)
        vkmap.put(KeyEvent.KEYCODE_COMMA, 0xBC); // VK_OEM_COMMA
        vkmap.put(KeyEvent.KEYCODE_PERIOD, 0xBE); // VK_OEM_PERIOD
//        vkmap.put(KeyEvent.0x2C, 0xBF); // VK_OEM_2 (?)
//        vkmap.put(KeyEvent.0x32, 0xC0); // VK_OEM_3 (~)
//        vkmap.put(KeyEvent.KEYCODE_LEFT_BRACKET, 0xDB); // VK_OEM_4 ([{)
//        vkmap.put(KeyEvent.0x27, 0xDE);  // VK_OEM_7 (‘ »)

        this.setFocusable(true);
        this.setFocusableInTouchMode(true);

        // This call is necessary, or else the
        // draw method will not be called.
        setWillNotDraw(false);
    }

    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent event) {
        int actionIndex = event.getActionIndex();
        int action = event.getActionMasked();
        switch (action) {
        case MotionEvent.ACTION_DOWN:
        case MotionEvent.ACTION_POINTER_DOWN:
            //Log.d(TAG, "ACTION_DOWN/ACTION_POINTER_DOWN count: " + touchCount + ", actionIndex: " + actionIndex);
            //NativeLib.buttonDown((int)((event.getX(actionIndex) - screenOffsetX) / screenScaleX), (int)((event.getY(actionIndex) - screenOffsetY) / screenScaleY));
            NativeLib.buttonDown((int)((event.getX(actionIndex) - viewPanOffsetX) / viewScaleFactorX), (int)((event.getY(actionIndex) - viewPanOffsetY) / viewScaleFactorY));
            break;
        case MotionEvent.ACTION_UP:
        case MotionEvent.ACTION_POINTER_UP:
            //Log.d(TAG, "ACTION_UP/ACTION_POINTER_UP count: " + touchCount + ", actionIndex: " + actionIndex);
            //NativeLib.buttonUp((int)((event.getX(actionIndex) - screenOffsetX) / screenScaleX), (int)((event.getY(actionIndex) - screenOffsetY) / screenScaleY));
            NativeLib.buttonUp((int)((event.getX(actionIndex) - viewPanOffsetX) / viewScaleFactorX), (int)((event.getY(actionIndex) - viewPanOffsetY) / viewScaleFactorY));
            break;
        default:
            break;
        }
        //return true;
        return super.onTouchEvent(event);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if((event.getFlags() & KeyEvent.FLAG_VIRTUAL_HARD_KEY) == 0) {
            Integer windowsKeycode = vkmap.get(keyCode);
            if (windowsKeycode != null)
                NativeLib.keyDown(windowsKeycode);
            else
                Log.e(TAG, String.format("Unknown keyCode: %d", keyCode));
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if((event.getFlags() & KeyEvent.FLAG_VIRTUAL_HARD_KEY) == 0) {
            Integer windowsKeycode = vkmap.get(keyCode);
            if (windowsKeycode != null)
                NativeLib.keyUp(windowsKeycode);
            else
                Log.e(TAG, String.format("Unknown keyCode: %d", keyCode));
        }
        return super.onKeyUp(keyCode, event);
    }

    @Override
    protected void onSizeChanged(int viewWidth, int viewHeight, int oldViewWidth, int oldViewHeight) {
        super.onSizeChanged(viewWidth, viewHeight, oldViewWidth, oldViewHeight);

        if(!viewSized) {
            viewSized = true;
            if (bitmapMainScreen != null) {
                setVirtualSize(bitmapMainScreen.getWidth(), bitmapMainScreen.getHeight());
                resetViewport();
            }
        }
    }

//    private void calcTranslateAndScale(int viewWidth, int viewHeight) {
//        float imageSizeX = bitmapMainScreen.getWidth();
//        float imageSizeY = bitmapMainScreen.getHeight();
//
//        if(imageSizeX > 0 && imageSizeY > 0 && viewWidth > 0.0f && viewHeight > 0.0f) {
//            // Find the scale factor and the translate offset to fit and to center the image in the view bounds.
//            float translateX = 0.0f, translateY = 0.0f, scaleX = 1.0f, scaleY = 1.0f;
//
//            if(fillScreen) {
//                scaleX = viewWidth / imageSizeX;
//                scaleY = viewHeight / imageSizeY;
//            } else {
//                if(horizontalSwipe) {
//                    float alpha = Math.max(2.0f, (viewHeight * imageSizeX) / (viewWidth * imageSizeY));
//                    scaleX = scaleY = alpha * viewWidth / imageSizeX;
//                } else {
//                    float viewRatio = (float) viewHeight / (float) viewWidth;
//                    float imageRatio = imageSizeY / imageSizeX;
//                    if (viewRatio > imageRatio) {
//                        scaleX = scaleY = this.fixScale != 0.0f ? this.fixScale : viewWidth / imageSizeX;
//                        translateX = (viewWidth - scaleX * imageSizeX) / 2.0f; //0.0f;
//                        translateY = (viewHeight - scaleY * imageSizeY) / 2.0f;
//                    } else {
//                        scaleX = scaleY = this.fixScale != 0.0f ? this.fixScale : viewHeight / imageSizeY;
//                        translateX = (viewWidth - scaleX * imageSizeX) / 2.0f;
//                        translateY = (viewHeight - scaleY * imageSizeY) / 2.0f; //0.0f;
//                    }
//                }
//            }
//
//            screenScaleX = scaleX;
//            screenScaleY = scaleY;
//            screenOffsetX = translateX;
//            screenOffsetY = translateY;
//        }
//    }

    /**
     * Draw the score.
     * @param canvas The canvas to draw to coming from the View.onDraw() method.
     */
    @Override
    protected void onCustomDraw(Canvas canvas) {
        //Log.d(TAG, "onCustomDraw()");

        canvas.drawColor(getBackgroundColor());
        canvas.drawBitmap(bitmapMainScreen, 0, 0, paint);
    }
//    @Override
//    protected void onDraw(Canvas canvas) {
//        //Log.d(TAG, "PAINT onDraw() mIsScaling: " + mIsScaling + ", mIsPanning: " + mIsPanning + ", mIsFlinging: " + mIsFlinging);
//
//        canvas.drawColor(getBackgroundColor());
//
//        canvas.save();
//        canvas.translate(screenOffsetX, screenOffsetY);
//        canvas.scale(screenScaleX, screenScaleY);
//        canvas.drawBitmap(bitmapMainScreen, 0, 0, paint);
//        canvas.restore();
//    }

    final int CALLBACK_TYPE_INVALIDATE = 0;
    final int CALLBACK_TYPE_WINDOW_RESIZE = 1;

    int updateCallback(int type, int param1, int param2, String param3, String param4) {
        switch (type) {
            case CALLBACK_TYPE_INVALIDATE:
                //Log.d(TAG, "PAINT updateCallback() postInvalidate()");
                postInvalidate();
                break;
            case CALLBACK_TYPE_WINDOW_RESIZE:
                // New Bitmap size
                if(bitmapMainScreen == null || bitmapMainScreen.getWidth() != param1 || bitmapMainScreen.getHeight() != param2) {
                    //Log.d(TAG, "PAINT updateCallback() Bitmap.createBitmap(x: " + Math.max(1, param1) + ", y: " + Math.max(1, param2) + ")");
                    Bitmap  oldBitmapMainScreen = bitmapMainScreen;
                    bitmapMainScreen = Bitmap.createBitmap(Math.max(1, param1), Math.max(1, param2), Bitmap.Config.ARGB_8888);
                    int globalColor = NativeLib.getGlobalColor();
                    kmlBackgroundColor = Color.argb(255, (globalColor & 0x00FF0000) >> 16, (globalColor & 0x0000FF00) >> 8, globalColor & 0x000000FF);

                    bitmapMainScreen.eraseColor(getBackgroundColor());
                    NativeLib.changeBitmap(bitmapMainScreen);

                    if(oldBitmapMainScreen != null) {
                        oldBitmapMainScreen.recycle();
                    }
                    setVirtualSize(bitmapMainScreen.getWidth(), bitmapMainScreen.getHeight());
                    if(viewSized)
                        resetViewport();
                }
                //postInvalidate();
                break;
        }
        return -1;
    }

    public Bitmap getBitmapMainScreen() {
        return bitmapMainScreen;
    }

    public void setFillScreen(boolean fillScreen) {
        //this.fillScreen = fillScreen;
        //calcTranslateAndScale(getWidth(), getHeight());
        //postInvalidate();
        setFillBounds(fillScreen);
    }

    public void setBackgroundKmlColor(boolean useKmlBackgroundColor) {
        this.useKmlBackgroundColor = useKmlBackgroundColor;
        postInvalidate();
    }

    public void setBackgroundFallbackColor(int fallbackBackgroundColorType) {
        this.fallbackBackgroundColorType = fallbackBackgroundColorType;
        postInvalidate();
    }

    public void setStatusBarColor(int statusBarColor) {
        this.statusBarColor = statusBarColor;
    }



    private int getBackgroundColor() {
        if(useKmlBackgroundColor) {
            return kmlBackgroundColor;
        } else switch(fallbackBackgroundColorType) {
            case 0:
                return 0;
            case 1:
                return statusBarColor;
        }
        return 0;
    }
}
