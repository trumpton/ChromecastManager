#
#
#

SOURCES := chromecast_BINALERT1.ogg chromecast_BINALERT2.ogg \
		chromecast_BINOK1.ogg chromecast_BINOK2.ogg \
		chromecast_BINNO1.ogg chromecast_BINNO2.ogg \
		chromecast_BINSTART1.ogg chromecast_BINSTART2.ogg \
		chromecast_BINEND1.ogg chromecast_BINEND2.ogg \
		chromecast_BINICON.png chromecast_BINPP.png \
		chromecast_BINIMG1.jpg chromecast_BINIMG2.jpg \
		chromecast_HTMLHELP.html chromecast_HTMLTEST.html chromecast_HTMLSCRIPT.js \
		chromecast_HTMLCSS.css chromecast_HTML404.html chromecast_HTMLSAMPLEDEVICELIST.json \
                mixkit_TEST1.ogg mixkit_TEST2.ogg \
		chromecast_TEST3.ogg chromecast_TEST4.ogg

TXT2H := txt2h
BIN2H := bin2h

all: ${BIN2H} chromecast_BINALERT1.oggh chromecast_BINALERT2.oggh \
		chromecast_BINOK1.oggh chromecast_BINOK2.oggh \
		chromecast_BINNO1.oggh chromecast_BINNO2.oggh \
		chromecast_BINSTART1.oggh chromecast_BINSTART2.oggh \
		chromecast_BINEND1.oggh chromecast_BINEND2.oggh \
		mixkit_TEST1.oggh mixkit_TEST2.oggh \
		chromecast_TEST3.oggh chromecast_TEST4.oggh \
		chromecast_BINICON.pngh \
		chromecast_BINIMG1.jpgh chromecast_BINIMG2.jpgh \
		chromecast_BINPP.pngh ${TXT2H} lhtmlfs.a

#
#
#

HEADERS := ${SOURCES:.mp3:.mp3h} ${SOURCES:.png:.pngh} \
		${SOURCES:.html=.htmlh} ${SOURCES:.js=.jsh} \
		${SOURCES:.json=.jsonh} ${SOURCES:.css=.cssh}


lhtmlfs.a: html_filesystem.c ${HEADERS}
	gcc -c html_filesystem.c -o html_filesystem.o
	ar -rcs $@ html_filesystem.o

clean: 
	/bin/rm -f ${BIN2H} ${TXT2H} *.htmlh *.jsh *.jsonh *.cssh *.pngh *.jpgh *.mp3h *.oggh *.o lhtmlfs.a

opt = $(shell echo $@ | cut -d_ -f2 | cut -d. -f1)

%.htmlh: %.html
	./${TXT2H} ${opt} $^ $@

%.jsh: %.js
	./${TXT2H} ${opt} $^ $@

%.jsonh: %.json
	./${TXT2H} ${opt} $^ $@

%.cssh: %.css
	./${TXT2H} ${opt} $^ $@

%.mp3h: %.mp3
	./${BIN2H} ${opt} $^ $@

%.oggh: %.ogg
	./${BIN2H} ${opt} $^ $@

%.jpgh: %.jpg
	./${BIN2H} ${opt} $^ $@

%.pngh: %.png
	./${BIN2H} ${opt} $^ $@

${BIN2H}: ${BIN2H}.c
	gcc ${BIN2H}.c -o ${BIN2H}

${TXT2H}: ${TXT2H}.c
	gcc ${TXT2H}.c -o ${TXT2H}


