// Copyright (c) Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#import <AppKit/AppKit.h>
#include <unordered_map>

enum
{
	ViewerCameraKey_Forward,
	ViewerCameraKey_Back,
	ViewerCameraKey_Left,
	ViewerCameraKey_Right,
	ViewerCameraKey_Up,
	ViewerCameraKey_Down,
	ViewerCameraKey_Fast,
	ViewerCameraKey_Slow,
	ViewerCameraKey_Count,
};

struct JoltCViewer_MacOSInputState
{
	bool keys[ViewerCameraKey_Count];
	bool rightMouseDown;
	bool focusRequested;
	float mouseDeltaX;
	float mouseDeltaY;
	float wheelDelta;
};

static void JoltCViewer_SetMacOSKey(JoltCViewer_MacOSInputState* state, unsigned short keyCode, bool down)
{
	switch (keyCode)
	{
	case 0x0D: state->keys[ViewerCameraKey_Forward] = down; break; // W
	case 0x01: state->keys[ViewerCameraKey_Back] = down; break;    // S
	case 0x00: state->keys[ViewerCameraKey_Left] = down; break;    // A
	case 0x02: state->keys[ViewerCameraKey_Right] = down; break;   // D
	case 0x0E: state->keys[ViewerCameraKey_Up] = down; break;      // E
	case 0x0C: state->keys[ViewerCameraKey_Down] = down; break;    // Q
	case 0x03:                                                       // F
		if (down)
			state->focusRequested = true;
		break;
	default:
		break;
	}
}

static void JoltCViewer_UpdateMacOSModifiers(JoltCViewer_MacOSInputState* state, NSEventModifierFlags flags)
{
	state->keys[ViewerCameraKey_Fast] = (flags & NSEventModifierFlagShift) != 0;
	state->keys[ViewerCameraKey_Slow] = (flags & (NSEventModifierFlagControl | NSEventModifierFlagOption)) != 0;
}

static std::unordered_map<NSWindow*, JoltCViewer_MacOSInputState> sWindowInputStates;

static void JoltCViewer_FocusMacOSWindow(NSEvent* event)
{
	NSWindow* window = event.window;
	if (window == nil)
		return;

	[window makeKeyWindow];
	NSView* contentView = window.contentView;
	if (contentView != nil && [contentView acceptsFirstResponder])
		[window makeFirstResponder:contentView];
}

static NSWindow* JoltCViewer_GetEventWindow(NSApplication* app, NSEvent* event)
{
	NSWindow* window = event.window;
	return window != nil ? window : app.keyWindow;
}

static void JoltCViewer_HandleMacOSEvent(NSApplication* app, NSEvent* event)
{
	NSWindow* window = JoltCViewer_GetEventWindow(app, event);
	if (window == nil)
		return;

	JoltCViewer_MacOSInputState& state = sWindowInputStates[window];

	switch (event.type)
	{
	case NSEventTypeKeyDown:
		if (!event.isARepeat)
			JoltCViewer_SetMacOSKey(&state, event.keyCode, true);
		break;
	case NSEventTypeKeyUp:
		JoltCViewer_SetMacOSKey(&state, event.keyCode, false);
		break;
	case NSEventTypeFlagsChanged:
		JoltCViewer_UpdateMacOSModifiers(&state, event.modifierFlags);
		break;
	case NSEventTypeLeftMouseDown:
	case NSEventTypeOtherMouseDown:
		JoltCViewer_FocusMacOSWindow(event);
		break;
	case NSEventTypeRightMouseDown:
		JoltCViewer_FocusMacOSWindow(event);
		state.rightMouseDown = true;
		break;
	case NSEventTypeRightMouseUp:
		state.rightMouseDown = false;
		break;
	case NSEventTypeRightMouseDragged:
		if (state.rightMouseDown)
		{
			state.mouseDeltaX += event.deltaX;
			state.mouseDeltaY += event.deltaY;
		}
		break;
	case NSEventTypeScrollWheel:
		state.wheelDelta += event.scrollingDeltaY * 0.1f;
		break;
	default:
		break;
	}
}

bool JoltCViewer_MacOSPollEvents(void* nativeView, JoltCViewer_MacOSInputState* state)
{
	@autoreleasepool
	{
		NSView* view = (NSView*)nativeView;
		NSWindow* targetWindow = view != nil ? view.window : nil;
		NSApplication* app = [NSApplication sharedApplication];
		static bool sAppInitialized = false;
		if (!sAppInitialized)
		{
			[app setActivationPolicy:NSApplicationActivationPolicyRegular];
			sAppInitialized = true;
		}
		[app finishLaunching];

		for (;;)
		{
			NSEvent* event = [app nextEventMatchingMask:NSEventMaskAny
											 untilDate:[NSDate distantPast]
												inMode:NSDefaultRunLoopMode
											   dequeue:YES];
			if (event == nil)
				break;

			JoltCViewer_HandleMacOSEvent(app, event);
			[app sendEvent:event];
		}

		[app updateWindows];

		if (targetWindow != nil)
		{
			JoltCViewer_MacOSInputState& windowState = sWindowInputStates[targetWindow];
			*state = windowState;
			state->mouseDeltaX = windowState.mouseDeltaX;
			state->mouseDeltaY = windowState.mouseDeltaY;
			state->wheelDelta = windowState.wheelDelta;
			JoltCViewer_UpdateMacOSModifiers(state, [NSEvent modifierFlags]);
			windowState.mouseDeltaX = 0.0f;
			windowState.mouseDeltaY = 0.0f;
			windowState.wheelDelta = 0.0f;
			windowState.focusRequested = false;
		}

		return targetWindow != nil && [targetWindow isVisible];
	}
}
