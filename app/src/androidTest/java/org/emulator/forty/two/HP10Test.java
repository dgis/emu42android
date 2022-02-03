package org.emulator.forty.two;


import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressKey;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;
import static org.emulator.forty.two.TestUtil.getActivityInstance;
import static org.emulator.forty.two.TestUtil.sleepFor;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.anything;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;

import androidx.annotation.NonNull;
import androidx.test.espresso.DataInteraction;
import androidx.test.espresso.ViewInteraction;
import androidx.test.espresso.contrib.NavigationViewActions;
import androidx.test.filters.LargeTest;
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner;
import androidx.test.rule.ActivityTestRule;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

@LargeTest
@RunWith(AndroidJUnit4ClassRunner.class)
public class HP10Test {

	@Rule
	public ActivityTestRule<MainActivity> mActivityTestRule = new ActivityTestRule<>(MainActivity.class);

	//private UiDevice mDevice;



	@Test
	public void mainActivityTest3() {
		//mDevice = UiDevice.getInstance(getInstrumentation());

		ViewInteraction mainScreenView = createHP10();

		//onView(isRoot()).perform(waitFor(10));
		sleepFor(10);
		mainScreenView.perform(pressKey(0x8)); // 1
		sleepFor(10);
		mainScreenView.perform(pressKey(0x46)); // = -> +
		sleepFor(10);
		mainScreenView.perform(pressKey(0x9)); // 2
		sleepFor(10);
		mainScreenView.perform(pressKey(0x42)); // Enter

		ViewInteraction appCompatImageButton = onView(
				allOf(withId(R.id.button_menu), withContentDescription("Open the main menu"),
						childAtPosition(
								allOf(withId(R.id.main_screen_container),
										childAtPosition(
												withId(R.id.main_coordinator),
												0)),
								2),
						isDisplayed()));
		appCompatImageButton.perform(click());

		onView(withId(R.id.nav_view)).perform(NavigationViewActions.navigateTo(R.id.nav_copy_stack));

		Activity activity = getActivityInstance();
		android.content.ClipboardManager clipboard = (android.content.ClipboardManager)activity.getSystemService(Context.CLIPBOARD_SERVICE);
		String result = clipboard.getText().toString();
		assertThat("result", result.compareTo("3") == 0);

	}

	@NonNull
	private ViewInteraction createHP10() {
		ViewInteraction navigationMenuItemView = onView(
				allOf(withId(R.id.nav_new),
						childAtPosition(
								allOf(withId(R.id.design_navigation_view),
										childAtPosition(
												withId(R.id.nav_view),
												0)),
								1),
						isDisplayed()));
		navigationMenuItemView.perform(click());

		DataInteraction materialTextView = onData(anything())
				.inAdapterView(allOf(withId(R.id.select_dialog_listview),
						childAtPosition(
								withId(R.id.contentPanel),
								0)))
				.atPosition(0);
		materialTextView.perform(click());

		ViewInteraction materialButton = onView(allOf(withId(android.R.id.button2), withText("Cancel"),
				childAtPosition(childAtPosition(withId(R.id.buttonPanel),0),2)));
		materialButton.perform(scrollTo(), click());


		ViewInteraction view = onView(allOf(withId(R.id.main_screen_view), withParent(allOf(withId(R.id.main_screen_container), withParent(withId(R.id.main_coordinator)))),
				isDisplayed()));
		view.check(matches(isDisplayed()));
		return view;
	}

	private static Matcher<View> childAtPosition(
			final Matcher<View> parentMatcher, final int position) {

		return new TypeSafeMatcher<View>() {
			@Override
			public void describeTo(Description description) {
				description.appendText("Child at position " + position + " in parent ");
				parentMatcher.describeTo(description);
			}

			@Override
			public boolean matchesSafely(View view) {
				ViewParent parent = view.getParent();
				return parent instanceof ViewGroup && parentMatcher.matches(parent)
						&& view.equals(((ViewGroup) parent).getChildAt(position));
			}
		};
	}
}
