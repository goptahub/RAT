hermes:
	cl hermes.c ftp.c tp.c pkt.c encodeconv.c svcconf.c log.c exectask.c -D LOGGING /Fe:hermes.exe

clean:
	rm -f *.obj

clean_exe:
	rm -f *.obj *.exe
	
log:
	cl log_if.c log.c svcconf.c encodeconv.c

conf:
	cl svcconf_if.c svcconf.c -D CONSOLE_LOGGING

ftp:
	cl ftp_if.c ftp.c tp.c pkt.c encodeconv.c svcconf.c -D CONSOLE_LOGGING

exectask:
	cl exectask_if.c exectask.c svcconf.c log.c encodeconv.c -D CONSOLE_LOGGING

