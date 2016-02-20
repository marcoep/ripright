Name:           ripright
Version:        0.9
Release:        2%{?dist}
Summary:        Minimal CD ripper
Group:          Applications/Multimedia
License:        GPLv2+
URL:            http://www.mcternan.me.uk/ripright/
Source0:        http://www.mcternan.me.uk/ripright/software/%{name}-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires:  flac-devel >= 1.1.4 cdparanoia-devel libcurl-devel ImageMagick-devel

%description
RigRight is a minimal CD ripper with few options.  It can run as a daemon
and will automatically start ripping any  CD found in the drive after which
the disc will be ejected.  Ripping is always to FLAC lossless audio format
with tags taken from the MusicBrainz look-up service and cover art from Amazon
where possible.  If a disc is unknown to MusicBrainz, the CD will be ejected
without ripping.

%prep
%setup -q


%build
%configure --docdir=%{_defaultdocdir}/%{name}-%{version}
make %{?_smp_mflags}


%check
make check


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
# due to this entry, doc must not be used to add any other files
%{_defaultdocdir}/%{name}-%{version}/
%{_bindir}/ripright
%{_bindir}/riparrange
%{_mandir}/man1/ripright.1.*
%{_mandir}/man1/riparrange.1.*

%changelog
* Tue Dec 8 2015 Michael McTernan <Mike@McTernan.uk> 0.1-2
- Update contact email

* Sat May 7 2011 Michael McTernan <Michael.McTernan.2001@cs.bris.ac.uk> 0.1-1
- Initial version.

