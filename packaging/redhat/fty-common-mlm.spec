#
#    fty-common-mlm - Provides common Malamute and ZeroMQ tools for agents
#
#    Copyright (C) 2014 - 2018 Eaton
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License along
#    with this program; if not, write to the Free Software Foundation, Inc.,
#    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

# To build with draft APIs, use "--with drafts" in rpmbuild for local builds or add
#   Macros:
#   %_with_drafts 1
# at the BOTTOM of the OBS prjconf
%bcond_with drafts
%if %{with drafts}
%define DRAFTS yes
%else
%define DRAFTS no
%endif
Name:           fty-common-mlm
Version:        1.0.0
Release:        1
Summary:        provides common malamute and zeromq tools for agents
License:        GPL-2.0+
URL:            https://42ity.org
Source0:        %{name}-%{version}.tar.gz
Group:          System/Libraries
# Note: ghostscript is required by graphviz which is required by
#       asciidoc. On Fedora 24 the ghostscript dependencies cannot
#       be resolved automatically. Thus add working dependency here!
BuildRequires:  ghostscript
BuildRequires:  asciidoc
BuildRequires:  automake
BuildRequires:  autoconf
BuildRequires:  libtool
BuildRequires:  pkgconfig
BuildRequires:  xmlto
BuildRequires:  gcc-c++
BuildRequires:  libsodium-devel
BuildRequires:  zeromq-devel
BuildRequires:  czmq-devel
BuildRequires:  malamute-devel
BuildRequires:  cxxtools-devel
BuildRequires:  log4cplus-devel
BuildRequires:  fty-common-logging-devel
BuildRequires:  openssl-devel
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
fty-common-mlm provides common malamute and zeromq tools for agents.

%package -n libfty_common_mlm1
Group:          System/Libraries
Summary:        provides common malamute and zeromq tools for agents shared library

%description -n libfty_common_mlm1
This package contains shared library for fty-common-mlm: provides common malamute and zeromq tools for agents

%post -n libfty_common_mlm1 -p /sbin/ldconfig
%postun -n libfty_common_mlm1 -p /sbin/ldconfig

%files -n libfty_common_mlm1
%defattr(-,root,root)
%{_libdir}/libfty_common_mlm.so.*

%package devel
Summary:        provides common malamute and zeromq tools for agents
Group:          System/Libraries
Requires:       libfty_common_mlm1 = %{version}
Requires:       libsodium-devel
Requires:       zeromq-devel
Requires:       czmq-devel
Requires:       malamute-devel
Requires:       cxxtools-devel
Requires:       log4cplus-devel
Requires:       fty-common-logging-devel
Requires:       openssl-devel

%description devel
provides common malamute and zeromq tools for agents development tools
This package contains development files for fty-common-mlm: provides common malamute and zeromq tools for agents

%files devel
%defattr(-,root,root)
%{_includedir}/*
%{_libdir}/libfty_common_mlm.so
%{_libdir}/pkgconfig/libfty_common_mlm.pc
%{_mandir}/man3/*
%{_mandir}/man7/*

%prep

%setup -q

%build
sh autogen.sh
%{configure} --enable-drafts=%{DRAFTS}
make %{_smp_mflags}

%install
make install DESTDIR=%{buildroot} %{?_smp_mflags}

# remove static libraries
find %{buildroot} -name '*.a' | xargs rm -f
find %{buildroot} -name '*.la' | xargs rm -f


%changelog
