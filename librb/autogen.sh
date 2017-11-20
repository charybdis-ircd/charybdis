#! /bin/sh

TOP_DIR=$(dirname $0)

if test ! -f $TOP_DIR/configure.ac ; then
   echo "You must execute this script from the top level directory."
   exit 1
fi

AUTOCONF=${AUTOCONF:-autoconf}
ACLOCAL=${ACLOCAL:-aclocal}
AUTOMAKE=${AUTOMAKE:-automake}
AUTOHEADER=${AUTOHEADER:-autoheader}
LIBTOOLIZE=${LIBTOOLIZE:-libtoolize}
SHTOOLIZE=${SHTOOLIZE:-shtoolize}

run_or_die ()
{
   COMMAND=$1

   # check for empty commands
   if test -z "$COMMAND" ; then
      echo "No command specified."
      return 1
   fi

   shift;

   # print a message
   echo "Running $COMMAND $@"

   # run or die
   $COMMAND $@ ;
   if test $? -ne 0 ; then
      echo "$COMMAND failed. (exit code = $?)"
      exit 1
   fi

   return 0
}

cd $TOP_DIR

run_or_die $ACLOCAL -I ../m4
run_or_die $LIBTOOLIZE --force --copy
run_or_die $AUTOHEADER
run_or_die $AUTOCONF
run_or_die $AUTOMAKE --add-missing --copy
run_or_die $SHTOOLIZE all
