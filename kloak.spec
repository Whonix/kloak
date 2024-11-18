Name:           kloak
Version:        0.3.7
Release:        %autorelease
Summary:        Keystroke-level online anonymization kernel

License:        BSD-3-Clause
# vmonaco no longer maintains kloak, Whonix Project now actively maintains a fork.
# URL:            https://github.com/vmonaco/%%{name}
URL:            https://github.com/Whonix/%{name}
Source0:        %{url}/archive/%{version}-1/%{name}-%{version}-1.tar.gz

BuildRequires:  gcc
BuildRequires:  libubsan
BuildRequires:  make
BuildRequires:  pkgconf-pkg-config
BuildRequires:  pkgconfig(libevdev)
BuildRequires:  pkgconfig(libsodium)
BuildRequires:  pkgconfig(udev)
BuildRequires:  systemd-rpm-macros
Requires:       systemd-udev
%{?systemd_requires}

%description
A privacy tool that makes keystroke biometrics less effective. This is
accomplished by obfuscating the time intervals between key press and release
events, which are typically used for identification.

%prep
%autosetup -n %{name}-%{version}-1

%build
%make_build all

%install
%{__install} -Dm 0644 usr/lib/systemd/system/%{name}.service -t %{buildroot}%{_unitdir}
%{__install} -Dm 0644 lib/udev/rules.d/*.rules -t %{buildroot}%{_udevrulesdir}
%{__install} -Dm 0755 eventcap -t %{buildroot}%{_sbindir}
%{__install} -Dm 0755 %{name} -t %{buildroot}%{_sbindir}
%{__install} -Dm 0644 auto-generated-man-pages/*.8 -t %{buildroot}%{_mandir}/man8

%post
%systemd_post %{name}.service
%udev_rules_update

%preun
%systemd_preun %{name}.service

%postun
%systemd_postun_with_restart %{name}.service
%udev_rules_update

%files
%license COPYING LICENSE
%doc *.md
%{_unitdir}/%{name}.service
%{_udevrulesdir}/*-%{name}.rules
%{_sbindir}/eventcap
%{_sbindir}/%{name}
%{_mandir}/man8/eventcap.8*
%{_mandir}/man8/%{name}.8*

%changelog
%autochangelog
