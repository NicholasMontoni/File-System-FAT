#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "file-system.h"

int main(int argc, char *argv[]) {
    FileSystem* fs = loadFileSystem("filesystem", 4096);

    //TEST createFILE and eraseFile 
    createFile(fs, "file1.txt");
    createFile(fs, "file2.txt");
    createFile(fs, "file3.txt");
    createFile(fs, "file4.txt");
    createFile(fs, "file5.txt");

    //eraseFile(fs,"file4.txt");
    //eraseFile(fs, "file3.txt");
    //eraseFile(fs,"file1.txt");
    //eraseFile(fs, "file3.txt");
//
    //createFile(fs, "file1.txt");
    //createFile(fs, "file2.txt");
    //createFile(fs, "file3.txt");
    //createFile(fs, "file4.txt");


    //TEST openFile
    FileHandle* f1 = openFile(fs, "file1.txt");
    FileHandle* f2 = openFile(fs, "file2.txt");
    FileHandle* f3 = openFile(fs, "file3.txt");
    FileHandle* f7 = openFile(fs, "file7.txt");

    //TEST writeFile
    writeFile(fs, f1, "Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Aenean commodo ligula eget dolor. Aenean massa. Cum sociis natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. Donec quam felis, ultricies nec, pellentesque eu, pretium quis, sem. Nulla consequat massa quis enim. Donec pede justo, fringilla vel, aliquet nec, vulputate eget, arcu. In enim justo, rhoncus ut, imperdiet a, venenatis vitae, justo. Nullam dictum felis eu pede mollis pretium. Integer tincidunt. Cras dapibus.", 503);
    writeFile(fs, f3, "La mia anima è pervasa da una mirabile serenità, simile a queste belle mattinate di maggio che io godo con tutto il cuore. Sono solo e mi rallegro di vivere in questo luogo che sembra esser creato per anime simili alla mia. Sono così felice, mio caro, così immerso nel sentimento della mia tranquilla esistenza che la mia arte ne soffre. Non potrei disegnare nulla ora, neppure un segno potrei tracciare; eppure mai sono stato così gran pittore come in questo momento.", 474);
    
    appendFile(fs, f1, " Vivamus elementum semper nisi. Aenean vulputate eleifend tellus. Aenean leo ligula, porttitor eu, consequat vitae, eleifend ac, enim. Aliquam lorem ante, dapibus in, viverra quis, feugiat a, tellus. Phasellus viverra nulla ut metus varius laoreet. Quisque rutrum. Aenean imperdiet. Etiam ultricies nisi vel augue. Curabitur ullamcorper ultricies nisi. Nam eget dui. Etiam rhoncus. Maecenas tempus, tellus eget condimentum rhoncus, sem quam semper libero, sit amet adipiscing sem neque sed ipsum. Nam quam nunc, blandit vel, luctus pulvinar, hendrerit id, lorem. Maecenas nec odio et ante tincidunt tempus. Donec vitae sapien ut libero venenatis faucibus. Nullam quis ante. Etiam sit amet orci eget eros faucibus tincidunt. Duis leo. Sed fringilla mauris sit amet nibh. Donec sodales sagittis magna.", 800);
    appendFile(fs, f3, " Quando l'amata valle intorno a me si avvolge nei suoi vapori, e l'alto sole posa sulla mia foresta impenetrabilmente oscura, e solo alcuni raggi si spingono nell'interno sacrario, io mi stendo nell'erba alta presso il ruscello che scorre, e più vicino alla terra osservo mille multiformi erbette; allora sento più vicino al mio cuore brulicare tra gli steli il piccolo mondo degli innumerevoli, infiniti vermiciattoli e moscerini, e sento la presenza dell'Onnipossente che ci ha creati a sua immagine e ci tiene in una eterna gioia.", 536); 
    

    //TEST readFile and seekFile 
    seekFile(fs, f1, 0, 0);
    seekFile(fs, f3, -535, 2);
    seekFile(fs, f2, 10, 1);

    char* buf1 = (char*) malloc (1305);
    char* buf2 = (char*) malloc (540);

    readFile(fs, f1, buf1, 1305);
    readFile(fs, f3, buf2, 540);

    while(*buf1) {
        printf("%c", *buf1);
        buf1++;
    }
    printf("\n");

    while (*buf2) {
        printf("%c", *buf2);
        buf2++;
    }
    printf("\n");





//    //TEST createDir
//    createDir(fs, "dir1");
//    createDir(fs, "dir2");
//    createDir(fs, "dir3");
//
//    //listDir(fs);
//
//    //TEST changeDir
//    changeDir(fs, "dir2");
//    
//    createDir(fs,"dir4");
//    FileHandle* f5 = createFile(fs, "file5.txt");
//    createDir(fs,"dir5");
//    FileHandle* f6 = createFile(fs, "file6.txt");
//    createDir(fs,"dir6");
//
//    changeDir(fs, "dir6");
//
//    FileHandle* f7 = createFile(fs, "file7.txt");
//    createDir(fs,"dir7");
//    FileHandle* f8 = createFile(fs, "file8.txt");
//    createDir(fs,"dir8");
//
//    //listDir(fs);
//
//    //TEST eraseFile and eraseDir
//    changeDir(fs, "..");
//    changeDir(fs, "..");
//    eraseFile(fs, f1);
//
//
//    //listDir(fs);
//
//    changeDir(fs, "dir2");
//    changeDir(fs, "dir6");
//    writeFile(fs, f7, "Gregorio Samsa, svegliandosi una mattina da sogni agitati, si trovò trasformato, nel suo letto, in un enorme insetto immondo. Riposava sulla schiena, dura come una corazza, e sollevando un poco il capo vedeva il suo ventre arcuato, bruno e diviso in tanti segmenti ricurvi, in cima a cui la coperta da letto, vicina a scivolar giù tutta, si manteneva a fatica. Le gambe, numerose e sottili da far pietà, rispetto alla sua corporatura normale, tremolavano senza tregua in un confuso luccichio dinanzi ai suoi occhi. Cosa m’è avvenuto?", 540);
//    changeDir(fs, "..");
//    writeFile(fs, f5, "Li Europan lingues es membres del sam familie. Lor separat existentie es un myth. Por scientie, musica, sport etc, litot Europa usa li sam vocabular. Li lingues differe solmen in li grammatica, li pronunciation e li plu commun vocabules. Omnicos directe al desirabilite de un nov lingua franca: On refusa continuar payar custosi traductores. At solmen va esser necessi far uniform grammatica, pronunciation e plu sommun paroles. Ma quande lingues coalesce, li grammatica del resultant lingue es plu simplic e regulari quam ti del coalescent lingues. Li nov lingua franca va esser plu simplic e regulari quam li existent Europan lingues. It va esser tam simplic quam Occidental in fact, it va esser Occidental. A un Angleso it va semblar un simplificat Angles, quam un skeptic Cambridge amico dit me que Occidental es.Li Europan lingues es membres del sam familie. Lor separat existentie es un myth. Por scientie, musica, sport etc, litot Europa usa li sam vocabular. Li lingues differe solmen in li grammatica, li pronunciation e li plu commun vocabules. Omnicos directe al desirabilite de un nov lingua franca: On refusa continuar payar custosi traductores.", 1159);
//    changeDir(fs, "dir6");
//    writeFile(fs, f7, " pensò. Non era un sogno. La sua camera, una stanzetta di giuste proporzioni, soltanto un po’ piccola, se ne stava tranquilla fra le quattro ben note pareti. Sulla tavola, un campionario disfatto di tessuti - Samsa era commesso viaggiatore e sopra, appeso alla parete, un ritratto, ritagliato da lui - non era molto - da una rivista illustrata e messo dentro una bella cornice dorata: raffigurava una donna seduta, ma ben dritta sul busto, con un berretto e un boa di pelliccia; essa levava incontro a chi guardava un pesante manicotto, in cui scompariva tutto l’avambraccio.", 581);
//
//    /*printf("[");
//    for (int i=0; i<NUM_BLOCKS; i++) {
//        printf("%d, ", fs->FAT[i]);
//    }
//    printf("]\n");
//    
//    /*for (int i=0; i<NUM_BLOCKS*5; i++) {
//        printf("%c, ", fs->data[i]);
//    }
//    printf("\n");  
//    printf("\n");  
//
//    changeDir(fs, "..");
//    eraseDir(fs, "dir6");
//
//    for (int i=0; i<NUM_BLOCKS*5; i++) {
//        printf("%c, ", fs->data[i]);
//    }
//    printf("\n");*/  
//
//
//    //Cleaning all
//    changeDir(fs, "..");
//    changeDir(fs, "..");
//    eraseDir(fs, "dir1");
//    eraseDir(fs, "dir2");
//    eraseDir(fs, "dir3");
//    eraseFile(fs, f2);
//    eraseFile(fs, f3);
//    eraseFile(fs, f4);
// 
//    free(buf1); free(buf2);
//    endFileSystem(fs);

    unloadFileSystem(fs);
}