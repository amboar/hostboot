# IBM_PROLOG_BEGIN_TAG
# This is an automatically generated prolog.
#
# $Source: env.bash $
#
# OpenPOWER HostBoot Project
#
# Contributors Listed Below - COPYRIGHT 2010,2015
# [+] International Business Machines Corp.
#
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied. See the License for the specific language governing
# permissions and limitations under the License.
#
# IBM_PROLOG_END_TAG
ROOTDIR=.

if [ -e ./customrc ]; then
    source ./customrc
fi

if [ -n "${OPENPOWER_BUILD}" ]; then
    export SKIP_BINARY_FILES=1
    export JAILCMD=""
else
    export FAKEROOT=${FAKEROOT:-/opt/mcp/shared/powerpc64-gcc-20150516}
    export CROSS_PREFIX=${CROSS_PREFIX:-${FAKEROOT}/wrappers/powerpc64-unknown-linux-gnu-}
    export HOST_PREFIX=${HOST_PREFIX:-${FAKEROOT}/wrappers/x86_64-pc-linux-gnu-}
    export PATH=${FAKEROOT}/wrappers:${PATH}
fi

# Setup some global variables
export PROJECT_NAME=HostBoot
export PROJECT_ROOT=$PWD
export TOOLSDIR=$PROJECT_ROOT/src/build/tools
export HOOKSDIR=$PROJECT_ROOT/.git/hooks
# Copyright license file for project
export LICENSE=$PROJECT_ROOT/LICENSE_PROLOG

# Update PATH
export PATH=${PATH}:$PWD/src/build/trace
export PATH=${PATH}:$TOOLSDIR

if [ -n "${SANDBOXROOT}" ]; then
    if [ -n "${SANDBOXNAME}" ]; then
        export SANDBOXBASE="${SANDBOXROOT}/${SANDBOXNAME}"
    fi
fi

export DEFAULT_MACHINE=MURANO

## Search and set gerrit host
# Gerrit host name should be in .ssh/config file
# Example:
# Host gerrit-server
#     Hostname gfw160.aus.stglabs.ibm.com
#     Port 29418
#     AFSTokenPassing no
if [ -e $HOME/.ssh/config ]; then
if [ -e $TOOLSDIR/gerrit-hostname ]; then
    echo "Searching for Gerrit Host..."
    eval $($TOOLSDIR/gerrit-hostname)
fi
fi

##  run setupgithooks.pl
if [ -e .git/hooks ]; then
if [ -e $TOOLSDIR/setupgithooks.sh ]; then
    $TOOLSDIR/setupgithooks.sh
fi
fi
