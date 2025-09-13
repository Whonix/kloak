# anti keystroke deanonymization tool #

An event-level online anonymization kernel for input devices

A privacy tool that makes keystroke and mouse biometrics less effective. For
keyboards, this is accomplished by obfuscating the time intervals between
input events. For mice, this is done by obfuscating the time intervals between
between mouse input events, the number of mouse input events, and the exact
path taken by the mouse pointer. This data can be used for identification if
not obfuscated.

kloak is designed for use on Wayland only. The way it functions could work on
X11 theoretically, but this is not implemented. Contributions welcome.

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
