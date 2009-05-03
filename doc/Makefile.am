ASCIIDOC=asciidoc
XMLTO=xmlto

dist_man_MANS = rsvndump.1

rsvndump.xml: rsvndump.txt
	$(ASCIIDOC) -b docbook -d manpage rsvndump.txt

rsvndump.1: rsvndump.xml
	$(XMLTO) man rsvndump.xml

distclean-local:
	$(RM) -f rsvndump.1
	$(RM) -f rsvndump.xml
