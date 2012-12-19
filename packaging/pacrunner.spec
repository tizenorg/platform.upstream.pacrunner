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
BuildRequires:  pkgconfig(libcurl)
%description
Pacrunner

%package test
Summary:        Test Scripts for pacrunner
Group:          Development/Tools
Requires:       %{name} = %{version}

%description test
Pacrunner tests



%prep
%setup -q

%build
%configure  --enable-v8 \
            --enable-curl \
            --enable-test
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


%files test
%{_libdir}/pacrunner/test/create-proxy-config
%{_libdir}/pacrunner/test/find-proxy-for-url

%changelog
