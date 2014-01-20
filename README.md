apps2desktop
============

NPAPI plugin for Google Chrome of Chromium, that creates desktop files for installed web applications


![](http://i.imgur.com/ykJ5FK3.jpg)


The plugin was tested on Fedora and Gnome 3. In Gnome 3 < 3.10 there are issues with STARTUP_WM_CLASS, so in window switcher (Alt+Tab) it acts like Chrome/Chromium window and not like a separate application.


Installation
------------

For installation use the install.sh script. As a prerequisite you have to have glib2-devel, json-glib-devel, vim-common (provides xxd needed by crxmake.sh), gcc and openssl installed. Package names are taken from Fedora for other distributions they can be different.

When you want to remove installed desktop files use the rmShortcuts.sh script.

For Fedora 20 we have prebuild .crx files available on http://tpopela.fedorapeople.org/
