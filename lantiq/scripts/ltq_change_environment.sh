#!/usr/bin/env bash

BASEDIR="."
ENVDIR="./config"
INITDIR=".."
ACTIVE=""
CONFIG_FIND_PATH="./config/"
CONFIG_USE_COPY=y
CONFIG_OVERRIDE_USER="/dev/null"
CONFIG_OVERRIDE_RESTORE="/dev/null"

#if [ -f ./scripts/ltq_change_ugw_version.sh ]; then
#	./scripts/ltq_change_ugw_version.sh x
#	if [ $? -ne 0 ]; then exit 1; fi
#fi

active_ugw_version=`if [ -f .active_ugw_version ]; then cat .active_ugw_version; fi`

[ -f "$BASEDIR/active_config" ] && {
	ACTIVE=`cat $BASEDIR/active_config`
}

[ -f "$BASEDIR/other_config_path" ] && {
	CONFIG_FIND_PATH=`cat $BASEDIR/other_config_path`
	CONFIG_USE_COPY=n
}

[ -f $BASEDIR/.config.override.user ] && {
	CONFIG_OVERRIDE_USER="$BASEDIR/.config.override.user"
}

[ -f $BASEDIR/.config.override.restore ] && {
	CONFIG_OVERRIDE_RESTORE="$BASEDIR/.config.override.restore"
}

usage() {
	cat <<EOF
Usage: $0 [options] <command> [arguments]
Commands:
	help              This help text
	list              List environments
	clear             Delete all environment and revert to flat config/files
	new <name>        Create a new environment
	switch <name>     Switch to a different environment
	delete <name>     Delete an environment
	rename <newname>  Rename the current environment
	diff              Show differences between current state and environment
	save              Save your changes to the environment
	revert            Revert your changes since last save
	checkin           Checkin to ClearCase

Options:

EOF
	exit ${1:-1}
}

error() {
	echo "$0: ERROR: $*"
	exit 1
}
warn() {
	echo "$0: WARNING: $*"
}

ask_bool() {
	local DEFAULT="$1"; shift
	local def defstr val
	case "$DEFAULT" in
		1) def=0; defstr="Y/n";;
		0) def=1; defstr="y/N";;
		*) def=;  defstr="y/n";;
	esac
	while [ -z "$val" ]; do
		local VAL

		echo -n "$* ($defstr): "
		read VAL
		case "$VAL" in
			y*|Y*) val=0;;
			n*|N*) val=1;;
			*) val="$def";;
		esac
	done
	return "$val"
}

ask_env() {
	local i k name
	printf "[0]\tAbort\n"
	k=1
	for envs in `find $CONFIG_FIND_PATH -type f -name .config -follow | sort -d`
	do
		name=`dirname $envs`
		arch=`cat $envs | grep CONFIG_ARCH= | sed 's/"/ /g' | cut -d' ' -f2`
		gcc=`cat $envs | grep GCC_VERSION= | sed 's/"/ /g' | cut -d' ' -f2`
		bin=`cat $envs | grep CONFIG_BINUTILS_VERSION= | sed 's/"/ /g' | cut -d' ' -f2`
		uclibc=`cat $envs | grep CONFIG_UCLIBC_VERSION= | sed 's/"/ /g' | cut -d' ' -f2`
		uclibcextra=`cat $envs | grep CONFIG_UCLIBC_EXTRA_VERSION= | sed 's/"/ /g' | cut -d' ' -f2`
		printf "[%d]\t%s (%s / %s / %s / %s)\n" $k $name $arch $gcc $bin $uclibc
		let k=$k+1
	done
	let k=$k-1
	printf "choice[0-%d?]: " $k
	read i
	if [ -z $i ] ; then
		echo "aborting... (no selection)"
		exit 1;
	fi
	if [ $i -eq 0 ] ; then
		echo "aborting... (selecting 0)"
		exit 1;
	fi
	if [ $i -gt $k ] ; then
		echo "aborting... (out of range: $i)"
		exit 1;
	fi
	k=1
	for envs in `find $CONFIG_FIND_PATH -type f -name .config -follow | sort -d`
	do
		if [ $i -eq $k ]; then
			name=`dirname $envs`
			env_link_config $name
			return 0
		fi
		let k=$k+1
	done
	return 1
}

env_link_config() {
	local NAME="$1"
	[ -f "$NAME/.config" ] || error "$NAME is invalid environment directory"
	rm -f "$BASEDIR/.config"
	rm -Rf "$BASEDIR/files"
		$BASEDIR/scripts/kconfig.pl "+" "$NAME/.config" $CONFIG_OVERRIDE_USER > "$BASEDIR/.config" || error "Failed to copy environment configuration"
	[ -d "$NAME/files" ] && {
		cp -Rf "$NAME/files" "$BASEDIR/files" || error "Failed to copy environment files"
		chmod -R u+wr "$BASEDIR/files" || error "Failed to change the protection"
	}
	echo $NAME > "$BASEDIR/active_config"
}

env_init_1() { # Made this function unused. Donot do copy operation.
	if [ $CONFIG_USE_COPY = "y" ]; then
		[ -d "$ENVDIR" ] || {
			mkdir -p "$ENVDIR"
			for dir in `ls "$INITDIR" | grep _config_`
			do
				if [ ! -z $active_ugw_version ]; then
					flt_cfgs=`cd $INITDIR; find "$dir" -name .config | grep -v ugw_`
					for flt in $flt_cfgs; do
						d_flt=`dirname $flt`
						mkdir -p $ENVDIR/$d_flt
						cp -f $INITDIR/$flt $ENVDIR/$d_flt/
					done; unset flt_cfgs flt d_flt;
					flt_cfgs=`cd $INITDIR; find "$dir" -name .config | grep $active_ugw_version`
					for flt in $flt_cfgs; do
						d_flt=`dirname $flt | sed 's/'"$active_ugw_version"'//'`
						mkdir -p $ENVDIR/$d_flt
						cp -f $INITDIR/$flt $ENVDIR/$d_flt/
					done; unset flt_cfgs flt d_flt;
				else
					cp -Rf "$INITDIR/$dir" "$ENVDIR" || error "Failed to initialize the environment directory"
				fi
			done
		}
	fi
}

env_init () {
	# Do nothing. use .config files from toplevel config dir.
	# Now toplevel config will be permenant link or dir.
	echo -n;
}

env_save() {
	[ ! -f "$BASEDIR/active_config" ] && error "No active environment found."
	[ -d "$ACTIVE/" ] || error "Can't save environment, directory $INITDIR does not exist."
	chmod -Rf u+wr "$ACTIVE/" || warn "Failed to change the protection"
	$BASEDIR/scripts/kconfig.pl "+" "$BASEDIR/.config" $CONFIG_OVERRIDE_RESTORE > "$ACTIVE/.config" || error "Failed to save the active environment"
	[ -d "$BASEDIR/files" ] && {
		cp -Rf "$BASEDIR/files" "$ACTIVE/"  || error "Failed to copy environment files"
	}
}

env_revert() {
	[ -z "$ACTIVE" ] && error "No active environment found."
	name=`echo "$INITDIR/$ACTIVE" | sed 's/\.\/config\///g'`
	[ -d "$name" ] || error "Can't revert environment, directory $name does not exist."
	[ -d "$ACTIVE" ] && {
		chmod -Rf u+wr "$ACTIVE" || error "Failed to change the protection"
		rm -Rf "$ACTIVE/"
	}
	mkdir -p "$ACTIVE/"
	cp -R "$name/.config" "$ACTIVE/.config" || error "Failed to revert the active environment"
	cp -Rf "$name/files" "$ACTIVE/" || error "Failed to revert the active environment"
}

env_checkin() {
	[ -z "$ACTIVE" ] && error "No active environment found."
	name=`echo "$INITDIR/$ACTIVE" | sed 's/\.\/config\///g'`
	[ -d "$name" ] || error "Can't checkin environment, directory $name does not exist."
	cleartool co -nc "$name/.config" || {
		cleartool unco "$name/.config"
		error "Failed to checkout the active environment"
	}
	cp -R "$ACTIVE/.config" "$name/.config" || error "Failed to copy the active environment"
	cleartool ci -nc "$name/.config" || error "Failed to checkin the active environment"
}

env_list() {
	env_init
	for envs in `find $CONFIG_FIND_PATH -type f -name .config -follow | sort -d`
	do
		name=`dirname $envs`
		if [ "$ACTIVE" != "" -a "$ACTIVE" = "$name" ]; then
			printf " * "
		else
			printf "   "
		fi
		printf "%s\n" "$name"
	done
}

env_new() {
	local NAME="$1"
	[ -z "$NAME" ] && usage
	env_init
	[ -f "$NAME/.config" ] && error "The configuration $NAME already exists."
	mkdir -p "$NAME/files"
	if [ -f "$BASEDIR/.config" ]; then
		if ask_bool 0 "Do you want to clone the current environment?"; then
			cp -f "$BASEDIR/.config" "$NAME/.config"
			[ -d "$BASEDIR/files" ] && cp -Rf "$BASEDIR/files/" "$NAME/files/"
		fi
	fi
	[ -f "$NAME/.config" ] || touch "$ENVDIR/$NAME/.config"
	rm -f "$BASEDIR/.config" "$BASEDIR/files"
	env_link_config $NAME
}

env_switch() {
	local NAME="$1"
	env_init
	[ -z "$NAME" ] && {
		ask_env
	} || {
		env_link_config $NAME
	}
}

env_delete() {
	local NAME="$1"
	[ -z "$NAME" ] && usage
	env_init
	[ -f "$BASEDIR/.config" ] &&
		[ "$ACTIVE" = "$NAME" ] && {
			if ask_bool 0 "Do you want delete the active anvironment?"; then
				rm -f "$BASEDIR/.config" "$BASEDIR/files"
				rm -Rf "$NAME/"
			fi
		} || {
			rm -Rf "$NAME/"
		}
}

env_rename() {
	local NAME="$1"
	[ -z "$NAME" ] && usage
	[ -z "$ACTIVE" ] && usage
	[ "$ACTIVE" = "$NAME" ] && error "Previous and new name are equal"
	rm -f "$BASEDIR/.config"
	rm -f "$BASEDIR/files"
	mv "$ACTIVE" "$NAME"
	env_link_config $NAME
}

env_diff() {
	[ -d "$BASEDIR/.config" ] && error "Can't find $BASEDIR/.config file"
	[ -z "$ACTIVE" ] && usage
	diff "$ACTIVE/.config" <($BASEDIR/scripts/kconfig.pl "+" "$BASEDIR/.config" $CONFIG_OVERRIDE_RESTORE)
}

COMMAND="$1"; shift
case "$COMMAND" in
	help) usage 0;;
	new) env_new "$@";;
	list) env_list "$@";;
	clear) env_clear "$@";;
	switch) env_switch "$@";;
	delete) env_delete "$@";;
	rename) env_rename "$@";;
	diff) env_diff "$@";;
	save) env_save "$@";;
	revert) env_revert "$@";;
	checkin) env_checkin "$@";;
	*) usage;;
esac
