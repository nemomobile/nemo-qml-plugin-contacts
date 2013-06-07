# 
# Do NOT Edit the Auto-generated Part!
# Generated by: spectacle version 0.26
# 

Name:       nemo-qml-plugin-contacts-qt5

# >> macros
# << macros

Summary:    Nemo QML contacts plugin
Version:    0.0.4
Release:    1
Group:      System/Libraries
License:    BSD
URL:        https://github.com/nemomobile/nemo-qml-plugin-contacts
Source0:    %{name}-%{version}.tar.bz2
Source100:  nemo-qml-plugin-contacts.yaml
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Declarative)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  pkgconfig(Qt5Versit)
BuildRequires:  pkgconfig(mlite5)

%description
%{summary}.

%package tests
Summary:    QML contacts plugin tests
Group:      System/Libraries
Requires:   %{name} = %{version}-%{release}

%description tests
%{summary}.

%prep
%setup -q -n %{name}-%{version}

# >> setup
# << setup

%build
# >> build pre
# << build pre

%qmake5

make %{?jobs:-j%jobs}

# >> build post
# << build post

%install
rm -rf %{buildroot}
# >> install pre
# << install pre
%qmake_install

# >> install post
# << install post

%files
%defattr(-,root,root,-)
%{_libdir}/qt5/qml/org/nemomobile/contacts/libnemocontacts.so
%{_libdir}/qt5/qml/org/nemomobile/contacts/qmldir
# >> files
# << files

%files tests
%defattr(-,root,root,-)
/opt/tests/nemo-qml-plugins/contacts-qt5/*
# >> files tests
# << files tests
