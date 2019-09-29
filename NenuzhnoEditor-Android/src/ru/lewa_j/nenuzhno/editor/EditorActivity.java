
package ru.lewa_j.nenuzhno.editor;

import android.app.Activity;
import android.widget.TextView;
import android.os.Bundle;
import android.opengl.*;
import android.view.*;
import ru.lewa_j.nenuzhno.engine.*;

public class EditorActivity extends Activity
{
	GLSurfaceView glView;
	WrapperRenderer glRenderer;
	
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
		requestWindowFeature(android.view.Window.FEATURE_NO_TITLE);
        
        glView = new GLSurfaceView(this);
		glView.setEGLContextClientVersion(2);

		glRenderer = new WrapperRenderer(glView);
		glView.setRenderer(glRenderer);

        setContentView(glView);
    }
    
    @Override
	public boolean onTouchEvent(MotionEvent event)
	{
		glRenderer.onTouchEvent(event);

		return super.onTouchEvent(event);
	}

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event)
	{
		glRenderer.onKey(keyCode,event.ACTION_DOWN);
		return super.onKeyDown(keyCode, event);
	}

	@Override
	public boolean onKeyUp(int keyCode, KeyEvent event)
	{
		glRenderer.onKey(keyCode,event.ACTION_UP);		
		return super.onKeyUp(keyCode, event);
	}
}
