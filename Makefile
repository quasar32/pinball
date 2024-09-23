pinball: glad/src/gl.o pinball.o sim.o draw.o
	gcc $^ -o $@ -lSDL2main -lSDL2 -lm -lswscale \
		-lavcodec -lavformat -lavutil -lx264

pinball.o: pinball.c sim.h draw.h
	gcc $< -o $@ -c -Iglad/include

sim.o: sim.c sim.h
	gcc $< -o $@ -c

draw.o: draw.c draw.h sim.h
	gcc $< -o $@ -c -Iglad/include -Icglm/include

glad/src/gl.o: glad/src/gl.c
	gcc $< -o $@ -c -Iglad/include 

clean:
	rm glad/src/gl.o *.o pinball 
