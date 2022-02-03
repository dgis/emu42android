package org.emulator.forty.two;

import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.runner.lifecycle.ActivityLifecycleMonitorRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matcher;

import java.util.Collection;
import java.util.Iterator;

public class TestUtil {

	public static Activity getActivityInstance(){
		final Activity[] currentActivity = {null};

		getInstrumentation().runOnMainSync(new Runnable(){
			public void run(){
				Collection<Activity> resumedActivity = ActivityLifecycleMonitorRegistry.getInstance().getActivitiesInStage(Stage.RESUMED);
				Iterator<Activity> it = resumedActivity.iterator();
				currentActivity[0] = it.next();
			}
		});

		return currentActivity[0];
	}

	/**
	 * Perform action of waiting for a specific time.
	 */
	public static ViewAction waitFor(final long millis) {
		return new ViewAction() {
			@Override
			public Matcher<View> getConstraints() {
				return isRoot();
			}

			@Override
			public String getDescription() {
				return "Wait for " + millis + " milliseconds.";
			}

			@Override
			public void perform(UiController uiController, final View view) {
				uiController.loopMainThreadForAtLeast(millis);
			}
		};
	}

	public static void sleepFor(final long millis) {
		try {
			Thread.sleep(millis);
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
	}
}
