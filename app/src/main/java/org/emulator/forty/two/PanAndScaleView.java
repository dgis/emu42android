package org.emulator.forty.two;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PointF;
import android.graphics.RectF;
import android.os.Handler;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.SurfaceView;
import android.view.View;
import android.widget.OverScroller;

import androidx.core.view.ViewCompat;

public class PanAndScaleView extends SurfaceView {
	
	protected RectF viewBounds = new RectF();
    protected PointF virtualSize = new PointF(0f, 0f);
	
    protected ScaleGestureDetector scaleDetector;
    protected GestureDetector gestureDetector;
    protected OverScroller scroller;
	protected boolean hasScrolled = false;
    protected PointF panPrevious = new PointF(0f, 0f);

    protected RectF rectScaleView = new RectF();
    protected RectF rectScaleImage = new RectF();

    protected float scaleFactorMin = 1.f;
    protected float scaleFactorMax = 8.f;
    protected float maxZoom = 8.f;
    protected float screenDensity = 1.0f;
	protected float scaleStep = (float)Math.sqrt(2.0);

    protected boolean showCursor = false;
    protected PointF cursorLocation = new PointF(0f, 0f);
	protected boolean showScaleThumbnail = false;
	protected boolean allowDoubleTapZoom = true;
	protected boolean fillBounds = false;

	protected Paint paint = null;
	protected OnClickCoordListener onClickCoordListener;

	public float viewPanOffsetX;
	public float viewPanOffsetY;
	public float viewScaleFactorX;
	public float viewScaleFactorY;

	protected float previousViewPanOffsetX;
	protected float previousViewPanOffsetY;
	protected float previousViewScaleFactorX;
	protected float previousViewScaleFactorY;

	protected boolean viewHasChanged() {
		if(viewPanOffsetX != previousViewPanOffsetX || viewPanOffsetY != previousViewPanOffsetY || viewScaleFactorX != previousViewScaleFactorX || viewScaleFactorY != previousViewScaleFactorY) {
			previousViewPanOffsetX = viewPanOffsetX;
			previousViewPanOffsetY = viewPanOffsetY;
			previousViewScaleFactorX = viewScaleFactorX;
			previousViewScaleFactorY = viewScaleFactorY;
			return true;
		} else
			return false;
	}

	public PanAndScaleView(Context context) {
		super(context);
		
		commonInitialize(context);
	}
	
	public PanAndScaleView(Context context, AttributeSet attrs) {
		super(context, attrs);
		
		commonInitialize(context);
	}
	
    /**
     * Interface definition for a callback to be invoked when a view is clicked.
     */
    public interface OnClickCoordListener {
        /**
         * Called when a view has been clicked.
         *
         * @param v The view that was clicked.
         */
        void onClick(View v, float x, float y);
    }
	
    /**
     * Register a callback to be invoked when this view is clicked. If this view is not
     * clickable, it becomes clickable.
     *
     * @param onClickCoordListener The callback that will run
     *
     * @see #setClickable(boolean)
     */
    public void setOnClickCoordListener(OnClickCoordListener onClickCoordListener) {
        this.onClickCoordListener = onClickCoordListener;
    }
    
    public float getMaxZoom() {
        return maxZoom;
    }
    
    public void setMaxZoom(float maxZoom) {
    	this.maxZoom = maxZoom;
    }
    
    public boolean getShowCursor() {
        return showCursor;
    }
    
    public void setShowCursor(boolean showCursor) {
        this.showCursor = showCursor;
    }
    
    public PointF getCursorLocation() {
        return cursorLocation;
    }

    public void setCursorLocation(float x, float y) {
        cursorLocation.set(x, y);
    }

	/**
	 * @return true to show a small scale thumbnail in the bottom right; false otherwise.
	 */
	public boolean getShowScaleThumbnail() {
		return showScaleThumbnail;
	}

	/**
	 * @param showScaleThumbnail true to show a small scale thumbnail in the bottom right; false otherwise.
	 */
	public void setShowScaleThumbnail(boolean showScaleThumbnail) {
		this.showScaleThumbnail = showScaleThumbnail;
	}

	/**
	 * @return true to allow to zoom when double tapping; false otherwise.
	 */
	public boolean getAllowDoubleTapZoom() {
		return allowDoubleTapZoom;
	}

	/**
	 * @param allowDoubleTapZoom true to allow to zoom when double tapping; false otherwise.
	 */
	public void setAllowDoubleTapZoom(boolean allowDoubleTapZoom) {
		this.allowDoubleTapZoom = allowDoubleTapZoom;
	}


	/**
	 * @return true to allow the virtual space to fill view bounds the ; false otherwise.
	 */
	public boolean getFillBounds() {
		return fillBounds;
	}

	/**
	 * @param fillBounds true to allow the virtual space to fill view bounds the ; false otherwise.
	 */
	public void setFillBounds(boolean fillBounds) {
		this.fillBounds = fillBounds;
		if(fillBounds) {
			viewPanOffsetX = 0.0f;
			viewPanOffsetY = 0.0f;
			viewScaleFactorX = viewBounds.width() / virtualSize.x;
			viewScaleFactorY = viewBounds.height() / virtualSize.y;
		} else {
			resetViewport();
		}
		invalidate();
	}



	public void resetViewport() {
		resetViewport(viewBounds.width(), viewBounds.height());
	}

	public void setVirtualSize(float width, float height) {
		
		virtualSize.x = width;
		virtualSize.y = height;
		//resetViewport();
	}

	
	
	private void commonInitialize(Context context) {

		paint = new Paint();
		paint.setColor(Color.BLACK);
		paint.setStyle(Paint.Style.STROKE);
		screenDensity = getResources().getDisplayMetrics().density;
		paint.setStrokeWidth(1.0f * screenDensity);
//		paint.setStrokeJoin(paint.Join.ROUND);
//		paint.setStrokeCap(paint.Cap.ROUND);
		//paint.setPathEffect(new CornerPathEffect(10));
		paint.setDither(true);
		paint.setAntiAlias(true);

		setPadding(0, 0, 0, 0);
		
		scroller = new OverScroller(context);
		
		scaleDetector = new ScaleGestureDetector(context, new ScaleGestureDetector.OnScaleGestureListener() {
		    @Override
		    public boolean onScaleBegin(ScaleGestureDetector detector) {
				return true;
			}

			@Override
		    public boolean onScale(ScaleGestureDetector detector) {
		    	if(fillBounds)
		    		return false;
				float scaleFactorPreviousX = viewScaleFactorX;
				float scaleFactorPreviousY = viewScaleFactorY;
				float detectorScaleFactor = detector.getScaleFactor();
				viewScaleFactorX *= detectorScaleFactor;
				viewScaleFactorY *= detectorScaleFactor;
			    constrainScale();
	            scaleWithFocus(detector.getFocusX(), detector.getFocusY(), scaleFactorPreviousX, scaleFactorPreviousY);
			    constrainPan();
		        invalidate();
		        return true;
		    }

		    @Override
	        public void onScaleEnd(ScaleGestureDetector detector) {
		    }
		});
		
		gestureDetector = new GestureDetector(context, new GestureDetector.SimpleOnGestureListener() {

			@Override
			public boolean onDown(MotionEvent e) {
				scroller.forceFinished(true);
				ViewCompat.postInvalidateOnAnimation(PanAndScaleView.this);
				return true;
			}

			@Override
			public boolean onDoubleTap(MotionEvent e) {
				if(!allowDoubleTapZoom || fillBounds)
					return false;
				float scaleFactorPreviousX = viewScaleFactorX;
				float scaleFactorPreviousY = viewScaleFactorY;
				viewScaleFactorX *= 2f;
				viewScaleFactorY *= 2f;
				if(viewScaleFactorX > scaleFactorMax || viewScaleFactorY > scaleFactorMax)
					viewScaleFactorX = viewScaleFactorY = scaleFactorMin;
				else {
					constrainScale();
		            scaleWithFocus(e.getX(), e.getY(), scaleFactorPreviousX, scaleFactorPreviousY);
				}
			    constrainPan();
		        invalidate();
				return true;
			}

			@Override
			public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
				if(fillBounds)
					return false;

				doScroll(distanceX, distanceY);
				return true;
			}

			@Override
			public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
				float viewPanMinX = viewBounds.width() - virtualSize.x * viewScaleFactorX;
				float viewPanMinY = viewBounds.height() - virtualSize.y * viewScaleFactorY;
				
				scroller.forceFinished(true);
				scroller.fling((int) viewPanOffsetX, (int) viewPanOffsetY, (int)-velocityX, (int)-velocityY, (int)viewPanMinX, 0, (int)viewPanMinY, 0);
				ViewCompat.postInvalidateOnAnimation(PanAndScaleView.this);
				return true;
			}
			
			@Override
			public boolean onSingleTapConfirmed(MotionEvent e) {
				if(onClickCoordListener != null) {
					float scaleAndPanX = (e.getX() - viewPanOffsetX) / viewScaleFactorX;
					float scaleAndPanY = (e.getY() - viewPanOffsetY) / viewScaleFactorY;
					onClickCoordListener.onClick(PanAndScaleView.this, scaleAndPanX, scaleAndPanY);
				}

				return super.onSingleTapConfirmed(e);
			}
		});
		
		this.setFocusable(true);
		this.setFocusableInTouchMode(true);

		// This call is necessary, or else the
		// draw method will not be called.
		setWillNotDraw(false);
	}

	@Override
	public void computeScroll() {
		if (scroller.computeScrollOffset() && !fillBounds) {
			
			//Log.i(VIEW_LOG_TAG, "computeScroll()");
			
			if (!hasScrolled) {
				panPrevious.x = scroller.getStartX();
				panPrevious.y = scroller.getStartY();
			}

			float deltaX = scroller.getCurrX() - panPrevious.x;
			float deltaY = scroller.getCurrY() - panPrevious.y;

			panPrevious.x = scroller.getCurrX();
			panPrevious.y = scroller.getCurrY();

			doScroll(deltaX, deltaY);
			hasScrolled = true;
		} else
			hasScrolled = false;
	}    

	public void doScroll(float deltaX, float deltaY) {

		viewPanOffsetX -= deltaX;
		viewPanOffsetY -= deltaY;
		constrainScale();
		constrainPan();
		invalidate();
	}

	public void postDoScroll(float deltaX, float deltaY, boolean center) {
		if(fillBounds)
			return;

		viewPanOffsetX -= deltaX;
		viewPanOffsetY -= deltaY;
		constrainScale();
		constrainPan(center);
		postInvalidate();
	}

	@SuppressLint("ClickableViewAccessibility")
	@Override
	public boolean onTouchEvent(MotionEvent event) {
		boolean retValScaleDetector = scaleDetector.onTouchEvent(event);
		boolean retValGestureDetector = gestureDetector.onTouchEvent(event);
		boolean retVal = retValGestureDetector || retValScaleDetector;

//	    int touchCount = event.getPointerCount();
//		if(touchCount == 1) {
//			int action = event.getAction();
//			switch (action) {
//			case MotionEvent.ACTION_DOWN:
//				
//			    panPrevious.x = event.getX(0);
//			    panPrevious.y = event.getY(0);
//			    mIsPanning = true;
//
//			    //Log.i(TAG, "touchesBegan count: " + touchCount + ", PanPrevious: " + panPrevious.toString());
//				
//				break;
//			case MotionEvent.ACTION_MOVE:
//				float panCurrentX = event.getX(0);
//				float panCurrentY = event.getY(0);
//				if(mIsPanning && Math.abs(panCurrentX - panPrevious.x) > 1.0f && Math.abs(panCurrentY - panPrevious.y) > 1.0f) {
//					mHasPanned = true;
//					mPanCurrent.x = panCurrentX;
//					mPanCurrent.y = panCurrentY;
//					viewPanOffsetX += mPanCurrent.x - panPrevious.x;
//					viewPanOffsetY += mPanCurrent.y - panPrevious.y;
//
////					Log.i(TAG, "Before viewPanOffsetX: " + viewPanOffsetX + ", viewPanOffsetY: " + viewPanOffsetY);
//					
//				    constrainScale();
//				    constrainPan();
//
////					Log.i(TAG, "After  viewPanOffsetX: " + viewPanOffsetX + ", viewPanOffsetY: " + viewPanOffsetY);
//
//				    //Log.i(TAG, "touchesMoved count: " + touchCount + ", PanOffset: " + mPanOffset.toString() + ", PanPrevious: " + panPrevious.toString() + ", PanCurrent: " + mPanCurrent.toString());
//					
//					panPrevious.x = mPanCurrent.x;
//					panPrevious.y = mPanCurrent.y;
//					
//			        invalidate();
//				}
//				break;
//			case MotionEvent.ACTION_UP:
//				if(!mHasScaled && !mHasPanned && onClickCoordListener != null) {
//					float scaleAndPanX = (event.getX() - viewPanOffsetX) / viewScaleFactor;
//					float scaleAndPanY = (event.getY() - viewPanOffsetY) / viewScaleFactor;
//					onClickCoordListener.onClick(this, scaleAndPanX, scaleAndPanY);
//				}
//				mIsPanning = false;
//				mHasPanned = false;
//				mHasScaled = false;
//				break;
//			case MotionEvent.ACTION_CANCEL:
//				break;
//			case MotionEvent.ACTION_OUTSIDE:
//				break;
//			default:
//			}
//		    return true;
//		}
        return retVal || super.onTouchEvent(event);
	}
	
	@Override
	public boolean onGenericMotionEvent(MotionEvent event) {
		if (!fillBounds && (event.getSource() & InputDevice.SOURCE_CLASS_POINTER) != 0) {
		//if (event.isFromSource(InputDevice.SOURCE_CLASS_POINTER)) {
			switch (event.getAction()) {
			case MotionEvent.ACTION_SCROLL:
				float wheelDelta = event.getAxisValue(MotionEvent.AXIS_VSCROLL);
		    	if(wheelDelta > 0f)
		    		scaleByStep(scaleStep, event.getX(), event.getY());
		    	else if(wheelDelta < 0f)
		    		scaleByStep(1.0f / scaleStep, event.getX(), event.getY());

				return true;
			}
		}
		return super.onGenericMotionEvent(event);
	}
	
	@Override
	public boolean onKeyUp(int keyCode, KeyEvent event) {
		if(!fillBounds) {
			char character = (char) event.getUnicodeChar();
			if (character == '+') {
				scaleByStep(scaleStep, getWidth() / 2.0f, getHeight() / 2.0f);
				return true;
			} else if (character == '-') {
				scaleByStep(1.0f / scaleStep, getWidth() / 2.0f, getHeight() / 2.0f);
				return true;
			}
		}
		return super.onKeyUp(keyCode, event);
	}
	
	/**
	 * Scale the view step by step following the mouse/touch position
	 * @param scaleFactor
	 * @param x
	 * @param y
	 */
	private void scaleByStep(float scaleFactor, float x, float y) {
		float scaleFactorPreviousX = viewScaleFactorX;
		float scaleFactorPreviousY = viewScaleFactorY;
		viewScaleFactorX *= scaleFactor;
		viewScaleFactorY *= scaleFactor;
		if(viewScaleFactorX > scaleFactorMax || viewScaleFactorY > scaleFactorMax)
			viewScaleFactorX = viewScaleFactorY = scaleFactorMax;
		else if(viewScaleFactorX < scaleFactorMin || viewScaleFactorY < scaleFactorMin)
			viewScaleFactorX = viewScaleFactorY = scaleFactorMin;
		else {
			constrainScale();
		    scaleWithFocus(x, y, scaleFactorPreviousX, scaleFactorPreviousY);
		}
		constrainPan();
		invalidate();
	}
	
	private void scaleWithFocus(float focusX, float focusY, float scaleFactorPreviousX, float scaleFactorPreviousY) {
		//float centeredScaleFactor = 1f - viewScaleFactor / scaleFactorPrevious;
		//viewPanOffsetX += (focusX - viewPanOffsetX) * centeredScaleFactor;
		//viewPanOffsetY += (focusY - viewPanOffsetY) * centeredScaleFactor;
		viewPanOffsetX = viewPanOffsetX + (focusX - viewPanOffsetX) - (focusX - viewPanOffsetX) * viewScaleFactorX / scaleFactorPreviousX;
		viewPanOffsetY = viewPanOffsetY + (focusY - viewPanOffsetY) - (focusY - viewPanOffsetY) * viewScaleFactorY / scaleFactorPreviousY;
	}

	private void constrainScale() {
        // Don't let the object get too small or too large.
		viewScaleFactorX = Math.max(scaleFactorMin, Math.min(viewScaleFactorX, scaleFactorMax));
		viewScaleFactorY = Math.max(scaleFactorMin, Math.min(viewScaleFactorY, scaleFactorMax));
	}
	private void constrainPan() {
		constrainPan(true);
	}
	private void constrainPan(boolean center) {

        // Keep the panning limits and the image centered.
		float viewWidth = viewBounds.width();
		float viewHeight = viewBounds.height();
		if(viewWidth == 0.0f){
			viewWidth = 1.0f;
			viewHeight = 1.0f;
		}
		float virtualWidth = virtualSize.x;
		float virtualHeight = virtualSize.y;
		if(virtualWidth == 0.0f){
			virtualWidth = 1.0f;
			virtualHeight = 1.0f;
		}
		float viewRatio = viewHeight / viewWidth;
		float imageRatio = virtualHeight / virtualWidth;
		float viewPanMinX = viewBounds.width() - virtualWidth * viewScaleFactorX;
		float viewPanMinY = viewBounds.height() - virtualHeight * viewScaleFactorY;
		if(viewRatio > imageRatio) {
	        // Keep the panning X limits.
			if(viewPanOffsetX < viewPanMinX)
				viewPanOffsetX = viewPanMinX;
			else if(viewPanOffsetX > 0f)
				viewPanOffsetX = 0f;
			
	        // Keep the image centered vertically.
			if(center && viewPanMinY > 0f)
				viewPanOffsetY = viewPanMinY / 2.0f;
			else {
				if(viewPanOffsetY > 0f)
					viewPanOffsetY = 0f;
				else if(viewPanOffsetY < viewPanMinY)
					viewPanOffsetY = viewPanMinY;
			}
		} else {
	        // Keep the panning Y limits.
			if(viewPanOffsetY < viewPanMinY)
				viewPanOffsetY = viewPanMinY;
			else if(viewPanOffsetY > 0f)
				viewPanOffsetY = 0f;

	        // Keep the image centered horizontally.
			if(center && viewPanMinX > 0f)
				viewPanOffsetX = viewPanMinX / 2.0f;
			else {
				if(viewPanOffsetX > 0f)
					viewPanOffsetX = 0f;
				else if(viewPanOffsetX < viewPanMinX)
					viewPanOffsetX = viewPanMinX;
			}
		}
	}

	public void resetViewport(float viewWidth, float viewHeight) {
		if(virtualSize.x > 0 && virtualSize.y > 0 && viewWidth > 0.0f && viewHeight > 0.0f) {
	    	// Find the scale factor and the translate offset to fit and to center the vectors in the view bounds. 
			float translateX, translateY, scale;
			float viewRatio = viewHeight / viewWidth;
			float imageRatio = virtualSize.y / virtualSize.x;
			if(viewRatio > imageRatio) {
				scale = viewWidth / virtualSize.x;
				translateX = 0.0f;
				translateY = (viewHeight - scale * virtualSize.y) / 2.0f;
			} else {
				scale = viewHeight / virtualSize.y;
				translateX = (viewWidth - scale * virtualSize.x) / 2.0f;
				translateY = 0.0f;
			}

			viewScaleFactorX = scale;
			viewScaleFactorY = scale;
	    	scaleFactorMin = scale;
		    scaleFactorMax = maxZoom * scaleFactorMin;
	    	viewPanOffsetX = translateX;
	    	viewPanOffsetY = translateY;
		}
	}


	boolean osdAllowed = false;
	Handler osdTimerHandler = new Handler();
	Runnable osdTimerRunnable = new Runnable() {
		@Override
		public void run() {
			// OSD should stop now!
			osdAllowed = false;
		}
	};

	void startOSDTimer() {
		osdTimerHandler.removeCallbacks(osdTimerRunnable);
		osdAllowed = true;
		osdTimerHandler.postDelayed(osdTimerRunnable, 500);
	}


	@Override
	protected void onSizeChanged(int viewWidth, int viewHeight, int oldViewWidth, int oldViewHeight) {
		super.onSizeChanged(viewWidth, viewHeight, oldViewWidth, oldViewHeight);
		
		viewBounds.set(0.0f, 0.0f, viewWidth, viewHeight);
		resetViewport((float)viewWidth, (float)viewHeight);
	}

	protected void onCustomDraw(Canvas canvas) {
	}

	@Override
    public void onDraw(Canvas canvas) {

        canvas.save();
        canvas.translate(viewPanOffsetX, viewPanOffsetY);
       	canvas.scale(viewScaleFactorX, viewScaleFactorY);
       	onCustomDraw(canvas);
        canvas.restore();
       	
       	if(showCursor) {
			paint.setColor(Color.RED);
			float cx = cursorLocation.x * viewScaleFactorX + viewPanOffsetX;
			float cy = cursorLocation.y * viewScaleFactorY + viewPanOffsetY;
       		float rayon = 2.0f * screenDensity;
       		float size = 10.0f * screenDensity;
       		canvas.drawLine(cx, cy - rayon - size, cx, cy - rayon, paint);
       		canvas.drawLine(cx - rayon - size, cy, cx - rayon, cy, paint);
       		canvas.drawLine(cx, cy + rayon + size, cx, cy + rayon, paint);
       		canvas.drawLine(cx + rayon + size, cy, cx + rayon, cy, paint);
       		canvas.drawCircle(cx, cy, rayon, paint);
       	}

		boolean viewHasChanged = viewHasChanged();
		if(viewHasChanged)
			startOSDTimer();

		if(!fillBounds && osdAllowed && showScaleThumbnail
		&& (viewScaleFactorX > scaleFactorMin || virtualSize.x > viewBounds.width() || virtualSize.y > viewBounds.height())) {
			// Draw the scale thumbnail
			paint.setColor(Color.RED);
			
			float scale = 0.2f;
			if(virtualSize.x > virtualSize.y)
				scale *= viewBounds.width() / virtualSize.x;
			else
				scale *= viewBounds.height() / virtualSize.y;
			float marginX = 10f * screenDensity, marginY = 10f * screenDensity;
	    	rectScaleImage.set(viewBounds.right - marginX - scale * virtualSize.x,
    			viewBounds.bottom - marginY - scale * virtualSize.y,
    			viewBounds.right - marginX,
    			viewBounds.bottom - marginY
	    	);
			canvas.drawRect(rectScaleImage, paint);
//    		rectBitmapSource.set(0, 0, (int) virtualSize.x, (int) virtualSize.y);
//    		if(mBitmap != null)
//    			canvas.drawBitmap(mBitmap, rectBitmapSource, rectScaleImage, paint);
    		rectScaleView.set(rectScaleImage.left + scale * (-viewPanOffsetX / viewScaleFactorX),
				rectScaleImage.top + scale * (-viewPanOffsetY / viewScaleFactorY),
				rectScaleImage.left + scale * (viewBounds.width() - viewPanOffsetX) / viewScaleFactorX,
				rectScaleImage.top + scale * (viewBounds.height() - viewPanOffsetY) / viewScaleFactorY
    		);
			canvas.drawRect(rectScaleView, paint);
		}
    }
}
