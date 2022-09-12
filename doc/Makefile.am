man_MANS = rsvndump.1

rsvndump.xml: rsvndump.txt
	$(ASCIIDOC) -b docbook -d manpage -a version=$(VERSION) -o $@ $<

rsvndump.1: rsvndump.xml
	$(XMLTO) man --skip-validation rsvndump.xml

distclean-local:
	$(RM) -f rsvndump.1
	$(RM) -f rsvndump.xml
