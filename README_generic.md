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

## How to install `kloak` using apt-get ##

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

## How to Build deb Package from Source Code ##

Can be build using standard Debian package build tools such as:

```
dpkg-buildpackage -b
```

See instructions.

NOTE: Replace `generic-package` with the actual name of this package `kloak`.

* **A)** [easy](https://www.whonix.org/wiki/Dev/Build_Documentation/generic-package/easy), _OR_
* **B)** [including verifying software signatures](https://www.whonix.org/wiki/Dev/Build_Documentation/generic-package)

## Contact ##

* [Free Forum Support](https://forums.whonix.org)
* [Premium Support](https://www.whonix.org/wiki/Premium_Support)

## Donate ##

`kloak` requires [donations](https://www.whonix.org/wiki/Donate) to stay alive!
