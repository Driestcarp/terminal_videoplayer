term_vid:
	g++ -O3 -o termvid terminal_videoplayer.cc `pkg-config --cflags --libs opencv4`