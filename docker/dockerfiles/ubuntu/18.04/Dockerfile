# vim: smartindent tabstop=4 shiftwidth=4 expandtab number
#
# Dockerfile to build Libsodium and Assimilation packages
#   All the packages we create are conveniently copied to /root/assimilation/packages
#
# This file is part of the Assimilation Project.
#
# Author: Alan Robertson <alanr@unix.sh>
# Copyright (C) 2014 - Assimilation Systems Limited
#
# Free support is available from the Assimilation Project community - http://assimproj.org
# Paid support is available from Assimilation Systems Limited - http://assimilationsystems.com
#
# The Assimilation software is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# The Assimilation software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with the Assimilation Project software.  If not, see http://www.gnu.org/licenses/
#
#
######################################################
#   Install required base packages
######################################################
FROM ubuntu:18.04
MAINTAINER Alan Robertson <alanr@assimilationsystems.com>
ENV TERM linux
ENV DEBIAN_FRONTEND noninteractive
RUN echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections

RUN apt-get -y update && apt-get -y install --no-install-recommends gcc cmake make pkg-config libglib2.0-dev resource-agents wget libpcap0.8-dev rsyslog 
RUN apt-get -y install --no-install-recommends python-pip python-flask python-setuptools debianutils lsof python-netaddr valgrind python-dev lsb-release python-demjson iproute2 adduser gnupg
RUN apt-get -y install --no-install-recommends bind9 host           # So we have some services to monitor in system testing
RUN apt-get -y install --no-install-recommends strace gdb tcpdump   # For debugging...
RUN pip install pytz
RUN pip install ctypesgen 'py2neo==2.0.8' getent pytest inject

###############################################################
#   Build and install Packages from Assimilation Source
###############################################################
RUN echo Sat Dec  1 00:37:45 PST 2018 # to make sure we always rebuild
COPY . /root/assimilation/src
RUN mkdir -p /root/assimilation/bin
RUN wget https://download.libsodium.org/libsodium/releases/LATEST.tar.gz && tar xvfz LATEST.tar.gz \
&& cd libsodium* && ./configure && make && make check && make install && ldconfig
RUN cd /root/assimilation/bin; cmake ../src && make install && cpack
