# privacy tool for anonymizing keyboard and mouse use #

A keystroke and mouse-level online anonymization kernel.
A tool to prevent tracking through keyboard and mouse input.

Real-time anonymization for input devices like keyboards and mice,
designed to counter tracking techniques.

Helps protect your privacy by making it harder to identify you based on
how you type or move your mouse.

For keyboards, it hides patterns by changing the timing between each key
press and release. These patterns, known as keystroke biometrics, could
otherwise be used to recognize individuals.

For mice, it introduces random changes to the timing and number of
movements, and modifies the mouse pointer path. This helps prevent
mouse biometrics from revealing identity.

kloak is designed for use only with the Wayland display server.

https://www.whonix.org/wiki/Keystroke_and_Mouse_Deanonymization#Kloak

Technical details:

* Obfuscates time intervals between keyboard input events.
* Obfuscates time intervals, frequency, and pointer paths for mouse input.

## Installation

There are two ways to run kloak:

  1. As an application
  2. As a Linux service

### As an application

Install dependencies:

Debian:

    $ sudo apt install make libevdev2 libevdev-dev libinput10 libinput-dev libwayland-client0 libwayland-dev libudev1 libudev-dev libxkbcommon0 libxkbcommon-dev pkg-config

Fedora:

Unknown currently. Refer to Debian dependency list and install equivalent packages on Fedora.

To compile `kloak`, simply run:

    $ make all

### As a service

How to install `kloak` using apt-get

1\. Download the APT Signing Key.

```
wget https://www.whonix.org/keys/derivative.asc
```

Users can [check the Signing Key](https://www.whonix.org/wiki/Signing_Key) for better security.

2\. Add the APT Signing Key.

```
sudo cp ~/derivative.asc /usr/share/keyrings/derivative.asc
```

3\. Add the derivative repository.

```
echo "deb [signed-by=/usr/share/keyrings/derivative.asc] https://deb.whonix.org trixie main contrib non-free" | sudo tee /etc/apt/sources.list.d/derivative.list
```

4\. Update your package lists.

```
sudo apt-get update
```

5\. Install `kloak`.

```
sudo apt-get install kloak
```

### How to build deb package

See the [Whonix package build documentation](https://www.whonix.org/wiki/Dev/Build_Documentation/security-misc). Replace the sample package name `security-misc` with `kloak` to download, build, and install kloak.

## Usage

Once `kloak` is compiled and installed, start it as root. This typically must run as root because `kloak` grabs exclusive access to input device files:

    $ sudo ./kloak

Then attempt to move the mouse. A red `+` sign should move around on the display. This is `kloak`'s mouse cursor. If you see this, `kloak` is running properly.

You can use the keyboard the same way you usually would. Keystrokes will not appear immediately when typed, but will be delayed by semi-random amounts of time. This is what provides the anonymity benefits of kloak.

The mouse can also be used the same as usual. When you move the mouse, only `kloak`'s mouse cursor will initially move; the operating system's cursor will "chase" it by jumping to `kloak`'s cursor's position at semi-random times. `kloak` will properly record where click events occur so that even if you click a location before the operating system's mouse cursor reaches that location, the click will register in the intended location.

If `kloak` is installed as a service, you should see the red `+` mouse cursor upon startup. All input events will be anonymized automatically.

## Whonix contact and support

* [Free Forum Support](https://forums.whonix.org)
* [Professional Support](https://www.whonix.org/wiki/Professional_Support)

## Donate

`kloak` requires [donations](https://www.whonix.org/wiki/Donate) to stay alive!

## Troubleshooting

### My keyboard or mouse seems very slow

`kloak` works by introducing a random delay to each input event (key press, key release, mouse move, etc). This requires temporarily buffering the event before it reaches the application (e.g., a text editor).

The maximum delay is specified with the -d option. This is the maximum delay (in milliseconds) that can occur between the physical input events and sending input events to the Wayland compositor. The default is 100 ms, which was shown to achieve about a 20-30% reduction in identification accuracy and doesn't create too much lag between the user and the application (see the paper below). As the maximum delay increases, the ability to obfuscate typing behavior also increases, and the responsiveness of the application decreases. This reflects a tradeoff between usability and privacy.

If you're a fast typist and it seems like there is a long lag between pressing a key and seeing the character on screen, try lowering the maximum delay. Alternatively, if you're a slower typist, you might be able to increase the maximum delay without noticing much difference. Automatically determining the best lag for each typing speed is an item for future work.

## Options

The full usage and options are:

    $ ./kloak -h

    Usage: kloak [options]
    Anonymizes keyboard and mouse input patterns by injecting jitter into input
    events. Designed specifically for wlroots-based Wayland compositors. Will NOT
    work with X11.

    Options:
      -d, --delay=milliseconds          maximum delay of released events.
                                        Default 100.
      -s, --start-delay=milliseconds    time to wait before startup. Default 500.
      -h, --help                        print help

## Try it out

See the [kloak defense testing](https://www.whonix.org/wiki/Keystroke_Deanonymization#Kloak) instructions.

## Background

`kloak` has two goals in mind:

* Make it difficult for an adversary to identify a user
* Make it difficult for an adversary to replicate a user's typing behavior

The first goal can theoretically be achieved only if all users cooperate with each other to have the same typing behavior, for example by pressing keys with exactly the same frequency. Since different users type at different speeds, this is not practical. Instead, pseudo-anonymity is achieved by obfuscating a user's typing rhythm, making it difficult for an adversary to re-identify a single user.

The second goal is to make it difficult for an adversary to forge typing behavior and impersonate a user, perhaps bypassing a two-factor authentication that uses keystroke biometrics. This is achieved by making the time between keystrokes unpredictable.

For more info, see the paper [Obfuscating Keystroke Time Intervals to Avoid Identification and Impersonation](https://arxiv.org/pdf/1609.07612.pdf).

### How it works

The time between key press and release events are typically used to identify users by their typing behavior. The pattern of mouse movements and clicks can be used in a similar fashion. `kloak` obfuscates these time intervals and patterns by introducing a random delay between the physical input events and the arrival of input events at the application, for example a web browser. For mice, the number of input events is also obfuscated by combining many small mouse move events into a few mouse jumps. This also obfuscates the exact shape of the mouse movement path.

kloak grabs the input device and sends delayed input events to the Wayland compositor using emulated input protocols. Grabbing the device disables any other application from reading the events. Events are scheduled to be released at a later time as they are received, and a semi-random delay is introduced before they are sent to the compositor.

### When does it fail

`kloak` does not protect against all forms of keystroke biometrics that can be used for identification. Specifically,

* If the delay is too small, it is not effective. Adjust the delay to as high a value that's comfortable.
* Repeated key presses are not obfuscated. If your system is set to repeat held-down keys at a unique rate, this could leak your identity. (TODO: Is this still the case, or are held-down keys obfuscated now?)
* Writing style is still apparent, in which [stylometry techniques could be used to determine authorship](https://vmonaco.com/papers/An%20investigation%20of%20keystroke%20and%20stylometry%20traits%20for%20authenticating%20online%20test%20takers.pdf).
* Higher-level cognitive behavior, such as editing and application usage, are still apparent. These lower-frequency actions are less understood at this point, but could potentially be used to reveal identity.
