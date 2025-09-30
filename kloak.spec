Name:           kloak
Version:        0.5.2
Release:        1%{?dist}
Summary:        Keystroke-level online anonymization kernel

License:        BSD-3-Clause
# vmonaco no longer maintains kloak, Whonix Project now actively maintains a fork.
# URL:            https://github.com/vmonaco/%%{name}
URL:            https://github.com/Whonix/%{name}
Source0:        https://deb.whonix.org/pool/main/k/%{name}/%{name}_%{version}.orig.tar.xz

BuildRequires:  gcc
BuildRequires:  libasan
BuildRequires:  libubsan
BuildRequires:  make
BuildRequires:  pkgconf-pkg-config
BuildRequires:  pkgconfig(libevdev)
BuildRequires:  pkgconfig(libinput)
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  pkgconfig(xkbcommon)
BuildRequires:  systemd-rpm-macros
%{?systemd_requires}

%description
A privacy tool that makes keystroke biometrics less effective. This is
accomplished by obfuscating the time intervals between key press and release
events, which are typically used for identification.

%prep
%autosetup -c

%build
%make_build

%install
# Using %%__install instead of %%make_install to avoid installation of apparmor profiles.
%{__install} -Dpm 0755 %{name} -t %{buildroot}%{_bindir}
%{__install} -Dpm 0644 usr/lib/systemd/system/%{name}.service -t %{buildroot}%{_unitdir}
%{__install} -Dpm 0755 usr/libexec/%{name}/* -t %{buildroot}%{_libexecdir}/%{name}/
%{__install} -Dpm 0644 auto-generated-man-pages/*.8 -t %{buildroot}%{_mandir}/man8

%post
%systemd_post %{name}.service

%preun
%systemd_preun %{name}.service

%postun
%systemd_postun_with_restart %{name}.service

%files
%license COPYING LICENSE
%doc *.md changelog.upstream
%{_bindir}/%{name}
%{_unitdir}/%{name}.service
%{_libexecdir}/%{name}/
%{_mandir}/man8/%{name}.8*

%changelog
* Tue Sep 30 2025 Jonathon Hyde <siliconwaffle@trilbyproject.org> - 0.5.2-1
- Added changelog.upstream
- New upstream version

* Tue Sep 23 2025 Jonathon Hyde <siliconwaffle@trilbyproject.org> - 0.5.0-1
- Initial RPM release
