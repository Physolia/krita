#! /bin/sh
$EXTRACTRC --tag=string *.ui >> rc.cpp
$XGETTEXT -kkundo2_i18nc:1c,2 -kkundo2_i18ncp:1c,2,3 *.cpp -o $podir/SpellCheckPlugin.pot
