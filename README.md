Just Tray Me
============

Configurable utility that allows to minimize any application to Windows system tray.

You can "trayify" any Windows application, including console applications. This is useful if you run things like `NGINX` or [Allog](https://github.com/pawelt/allog) that normally run in the background and you dont need to see their console windows all the time.

JTM can start, stop and toggle (show/hide) controlled application.


Installation.
-------------

Simply compile the source with Microsoft free tools and run `jtm.exe`.

Direct link to precompiled binaries: https://github.com/pawelt/just-tray-me/blob/master/binaries/jtm-x64.zip?raw=true

How to use
----------

Simply run the executable and pass a config file in command line, for example:

    D:\pawel\Programs\just-tray-me\jtm.exe D:\Pawel\Projects\just-tray-me\jtm-allog.ini

Simplest way would be to create a shortcut and put the config path in the shortuct's `Target` field.

The repo includes two example configs:

- `jtm-allog.ini` that can be used to control [Allog](https://github.com/pawelt/allog)
- `jtm-notepad.ini` show how to control notepad


How is JTM different?
---------------------

- it is open source and 100% free
- it allows to configure commands and parameters to start and stop controlled application
- very light and fast

