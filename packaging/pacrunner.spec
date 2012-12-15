Name:           pacrunner
Version:        0.5
Release:        0
License:        GPL-2.0+
Summary:        Proxy
Group:          Networking
Source:         %{name}-%{version}.tar.xz
BuildRequires:  v8-devel
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(glib-2.0)

%description
Pacrunner

%prep
%setup -q

%build
%configure
make %{?_smp_mflags}

%install
%make_install


%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root)
%license COPYING
%{_sysconfdir}/dbus-1/system.d/pacrunner.conf
/usr/sbin/pacrunner
/usr/share/dbus-1/system-services/pacrunner.service

%changelog
