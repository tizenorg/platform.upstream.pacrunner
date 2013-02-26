Name:           pacrunner
Version:        0.6
Release:        0
License:        GPL-2.0+
Url:            http://connman.net/
Summary:        Proxy configuration daemon
Group:          Connectivity/Connection Management
Source:         %{name}-%{version}.tar.xz
BuildRequires:  v8-devel
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(libcurl)
%description
PACrunner - Proxy configuration daemon.

%package test
Summary:        Test Scripts for pacrunner
Group:          Development/Tools
Requires:       %{name} = %{version}

%description test
PACrunner - Proxy configuration daemon.

Pacrunner tests

%package  proxy-tools
Summary:        Libproxy library Tools
Group:          Connectivity/Connection Management
Requires:       %{name}-libproxy = %{version}

%description proxy-tools
PACrunner - Proxy configuration daemon.

Libproxy library Tools.

%package libproxy
Summary:        Libproxy library
Group:          Connectivity/Connection Management
Requires:       %{name} = %{version}

%description libproxy
PACrunner - Proxy configuration daemon.

Libproxy library.

%package libproxy-devel
Summary:        Libproxy library Development Files
Group:          Development/Libraries
Requires:       pacrunner-libproxy = %{version}

%description  libproxy-devel
PACrunner - Proxy configuration daemon.

Libproxy library Development Files.

%prep
%setup -q

%build
%reconfigure --enable-v8 \
            --enable-libproxy \
            --enable-curl \
            --enable-test
make %{?_smp_mflags}

%install
%make_install


%post  libproxy -p /sbin/ldconfig

%postun  libproxy -p /sbin/ldconfig

%files
%defattr(-,root,root)
%license COPYING
%{_sysconfdir}/dbus-1/system.d/pacrunner.conf
/usr/sbin/pacrunner
/usr/share/dbus-1/system-services/org.pacrunner.service

%files  proxy-tools
%defattr(-,root,root)
%{_bindir}/proxy

%files  libproxy
%defattr(-,root,root)
%{_libdir}/libproxy.so.*

%files  libproxy-devel
%defattr(-,root,root)
%{_includedir}/proxy.h
%{_libdir}/libproxy.so
%{_libdir}/pkgconfig/libproxy-1.0.pc

%files test
%{_libdir}/pacrunner/test/create-proxy-config
%{_libdir}/pacrunner/test/find-proxy-for-url
%{_bindir}/manual-proxy-test

%changelog
