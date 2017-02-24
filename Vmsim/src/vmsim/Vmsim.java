/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package vmsim;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Hashtable;
import java.util.LinkedList;

/**
 *
 * @author Timothy Kang
 */
public class Vmsim {

    /**
     * @param args the command line arguments
     */
    public static void main(String[] args) throws FileNotFoundException, IOException {
        // TODO code application logic here

        //in case you TAs try to break some of my code
        if (args.length != 7) {
            System.out.println("Arguments that are propietary to this program have not been met");
            System.exit(0);
        }

        //getting information from arguments
        int frames = Integer.parseInt(args[1]);
        String method = args[3];
        int refreshRate = Integer.parseInt(args[5]);
        String traceFile = args[6];

        //read from file
        File file = new File(traceFile);
        FileReader fileReader = new FileReader(file);
        BufferedReader bufferedReader = new BufferedReader(fileReader);
        //StringBuffer stringBuffer = new StringBuffer();

        //useful stuff to hold track
        String line;
        int totalMemAccesses = 0;
        int totalPageFaults = 0;
        int totalWriteToDisks = 0;
        int frameCount = 0;
        int i = 0;
        boolean hit = false;

        //initialize pagetable with alotted frames
        PageTableEntry[] pageTable = new PageTableEntry[frames];

        //"optimal" performance
        //evict the page that will not be needed until furthest in the future
        if (method.equals("Opt")) {
            //need to see into the future so why not with 
            //look through and save each input with an index 
            //to see when its necessary
            String[] futureVision = new String[1024 * 1024];
            //an array that holds when the next address shows up

            //int[] lengthAway = new int[1024 * 1024];
            int futureIndex = 0;

            HashMap<Long, ArrayList<Integer>> hash = new HashMap<Long, ArrayList<Integer>>();

            while ((line = bufferedReader.readLine()) != null) {
                String[] parts = line.split(" ");
                Long address = Long.parseLong(parts[0], 16);
                futureVision[futureIndex] = line;
                if (hash.get(address) == null) {
                    hash.put(address, new ArrayList<Integer>());
                    hash.get(address).add(futureIndex);
                } else {
                    hash.get(address).add(futureIndex);
                 
                }

                futureIndex++;
            }
            int optRun = 0;
          
            int j;
            boolean found = false;
            int tempOptRun;

           
            while (optRun < futureIndex) {
                hit = false;
                String[] parts = futureVision[optRun].split(" ");

                PageTableEntry temp = new PageTableEntry();
                //temp.address = Integer.parseInt(parts[0]);
                temp.address = Long.parseLong(parts[0], 16);
                if (parts[1].equals("W")) {
                    temp.dirty = true;
                }
                //check to see if the referenced address is already in the pagetable
                for (i = 0; i < frameCount; i++) {
                    if (temp.address == pageTable[i].address) {
                        hit = true;
                        break;
                    }
                }

                //is a hit
                if (hit) {
                    if (parts[1].equals("W")) {
                        pageTable[i].dirty = true;
                    }
                    pageTable[i].referenced = true;
                  
                    System.out.println(parts[0] + " hit");
                    hash.get(pageTable[i].address).remove(0);

                } //no need to evict
                else if (frameCount < frames) {
                    //not a hit
                    if (!hit) {
                        temp.referenced = true;
                        pageTable[i] = temp;
                        if (parts[1].equals("W")) {
                            pageTable[i].dirty = true;
                           
                        } else {
                         
                        }
                     
                        frameCount++;
                        totalPageFaults++;
                        System.out.println(parts[0] + " page fault - no eviction");
                        hash.get(pageTable[i].address).remove(0);
                    }
                } //evict something using OPT
                else {
                    totalPageFaults++;
                    int maxIndex = 0;
                    //find the least recently used by see when the last use was
                    if (hash.get(pageTable[0].address).isEmpty()) {
                        //removes first index since it never occurs again

                    } else {

                        int max = hash.get(pageTable[0].address).get(0);

                        //find the max time of furthest used and evict
                        for (j = 0; j < frameCount; j++) {

                            if (hash.get(pageTable[j].address).isEmpty()) {
                                //removes first index since it never occurs again
                                maxIndex = j;
                                break;
                            } else {

                                if (max < hash.get(pageTable[j].address).get(0)) {
                                    maxIndex = j;

                                }
                            }
                        }
                    }
                    

                    if (pageTable[maxIndex].dirty == true) {
                        totalWriteToDisks++;
                        System.out.println(parts[0] + " page fault - evict dirty");
                    } else {
                        System.out.println(parts[0] + " page fault - evict clean");
                    }
                    pageTable[maxIndex] = temp;
                    
                    hash.get(pageTable[maxIndex].address).remove(0);

                }
              
                optRun++;
                totalMemAccesses++;

            }
        } //Clock method
        else if (method.equals("Clock")) {
            //clock to reference what index we are at
            int clockFrameIndex = 0;

            while ((line = bufferedReader.readLine()) != null) {
                hit = false;
                String[] parts = line.split(" ");
                PageTableEntry temp = new PageTableEntry();
                //temp.address = Integer.parseInt(parts[0]);
                temp.address = Long.parseLong(parts[0], 16);
                if (parts[1].equals("W")) {
                    temp.dirty = true;
                }
                //check to see if the referenced address is already in the pagetable
                for (i = 0; i < frameCount; i++) {
                    if (temp.address == pageTable[i].address) {
                        hit = true;
                        break;
                    }
                }

                //is a hit
                if (hit) {
                    if (parts[1].equals("W")) {
                        pageTable[i].dirty = true;
                    }
                    pageTable[i].referenced = true;
                    System.out.println(parts[0] + " hit");
                } //no need to evict
                else if (frameCount < frames) {
                    //not a hit
                    if (!hit) {
                        temp.referenced = true;
                        pageTable[frameCount] = temp;
                        frameCount++;
                        totalPageFaults++;
                        System.out.println(parts[0] + " page fault - no eviction");
                    }
                } //evict something using clock
                else {
                    totalPageFaults++;
                    while (true) {
                        //evict unreferenced page
                        if (pageTable[clockFrameIndex].referenced == false) {
                            if (pageTable[clockFrameIndex].dirty == true) {
                                totalWriteToDisks++;
                                System.out.println(parts[0] + " page fault - evict dirty");
                            } else {
                                System.out.println(parts[0] + " page fault - evict clean");
                            }
                            pageTable[clockFrameIndex] = temp;
                            pageTable[clockFrameIndex].referenced = true;
                            break;
                        } //unreference the page and move on
                        else {
                            pageTable[clockFrameIndex].referenced = false;
                        }
                        clockFrameIndex++;
                        if (clockFrameIndex == frames) {
                            clockFrameIndex = 0;
                        }
                    }
                    //due to the break we still need to add to the clock if something was freshly evicted and put in
                    clockFrameIndex++;
                    if (clockFrameIndex == frames) {
                        clockFrameIndex = 0;
                    }

                }
                totalMemAccesses++;

            }
        } else if (method.equals("NRU")) {
            //array to hold values when recently used
            int[] recent = new int[frames];
            int refresh = 0;
            //initialize all values
            int j = 0;
            for (j = 0; j < frames; j++) {
                recent[j] = 0;
            }
            while ((line = bufferedReader.readLine()) != null) {
                hit = false;
                String[] parts = line.split(" ");
                PageTableEntry temp = new PageTableEntry();
                //temp.address = Integer.parseInt(parts[0]);
                temp.address = Long.parseLong(parts[0], 16);
                if (parts[1].equals("W")) {
                    temp.dirty = true;
                }
                //check to see if the referenced address is already in the pagetable
                for (i = 0; i < frameCount; i++) {
                    if (temp.address == pageTable[i].address) {
                        hit = true;
                        break;
                    }
                }

                //is a hit
                if (hit) {
                    if (parts[1].equals("W")) {
                        pageTable[i].dirty = true;
                        recent[i] = 3;
                    } else {
                        recent[i] = 2;
                    }
                    pageTable[i].referenced = true;
                    System.out.println(parts[0] + " hit");

                } //no need to evict
                else if (frameCount < frames) {
                    //not a hit
                    if (!hit) {
                        temp.referenced = true;
                        pageTable[i] = temp;
                        if (parts[1].equals("W")) {
                            pageTable[i].dirty = true;
                            recent[i] = 3;
                        } else {
                            recent[i] = 2;
                        }
                        frameCount++;
                        totalPageFaults++;
                        System.out.println(parts[0] + " page fault - no eviction");
                    }
                } //evict something using NRU
                else {
                    totalPageFaults++;

                    //find the least recently used by see when the last use was 
                    int min = recent[0];
                    int minIndex = 0;
                    //find the max time of furthest used and evict
                    for (j = 0; j < frameCount; j++) {
                        if (min > recent[j]) {
                            minIndex = j;
                        }
                    }
                    if (pageTable[minIndex].dirty == true) {
                        totalWriteToDisks++;
                        System.out.println(parts[0] + " page fault - evict dirty");
                    } else {
                        System.out.println(parts[0] + " page fault - evict clean");
                    }
                    pageTable[minIndex] = temp;
                    if (pageTable[minIndex].dirty == true) {
                        //pageTable[j].dirty = true;
                        recent[minIndex] = 3;
                    } else {
                        recent[minIndex] = 2;
                    }
                    refresh++;
                    if (refresh > refreshRate) {
                        for (j = 0; j < frameCount; j++) {
                            if (pageTable[j].dirty == true) {
                                //pageTable[j].dirty = true;
                                recent[j] = 1;
                            } else {
                                recent[j] = 0;
                            }
                        }
                        refresh = 0;
                    }
                }
                if (refresh > refreshRate) {
                    for (j = 0; j < frameCount; j++) {
                        if (pageTable[j].dirty == true) {
                            //pageTable[j].dirty = true;
                            recent[j] = 1;
                        } else {
                            recent[j] = 0;
                        }
                    }
                    refresh = 0;
                }

                totalMemAccesses++;

            }
        } else if (method.equals("Aging")) {
            //array to hold values of binary values that need to be shifted
            int[] recent = new int[frames];
            //initialize all values as 2 to the amount of frames power but in the beginning we want them 0
            int j = 0;
            for (j = 0; j < frames; j++) {
                recent[j] = 0;
                //recent[j] = (int) Math.pow(2, frames);
            }
            while ((line = bufferedReader.readLine()) != null) {
                hit = false;
                String[] parts = line.split(" ");
                PageTableEntry temp = new PageTableEntry();
                //temp.address = Integer.parseInt(parts[0]);
                temp.address = Long.parseLong(parts[0], 16);
                if (parts[1].equals("W")) {
                    temp.dirty = true;
                }
                //check to see if the referenced address is already in the pagetable
                for (i = 0; i < frameCount; i++) {
                    if (temp.address == pageTable[i].address) {
                        hit = true;
                        break;
                    }
                }

                //is a hit
                if (hit) {
                    if (parts[1].equals("W")) {
                        pageTable[i].dirty = true;
                    }
                    pageTable[i].referenced = true;
                    System.out.println(parts[0] + " hit");
                    recent[i] += (int) Math.pow(2, frames);
                } //no need to evict
                else if (frameCount < frames) {
                    //not a hit
                    if (!hit) {
                        temp.referenced = true;
                        pageTable[i] = temp;
                        recent[i] += (int) Math.pow(2, frames);
                        frameCount++;
                        totalPageFaults++;
                        System.out.println(parts[0] + " page fault - no eviction");
                    }
                } //evict something using NRU
                else {
                    totalPageFaults++;

                    //want to evict the smallest value that has aged without getting used
                    int min = recent[0];
                    int minIndex = 0;
                    //find the max time of furthest used and evict
                    for (j = 0; j < frameCount; j++) {
                        if (min > recent[j]) {
                            minIndex = j;
                        }
                    }
                    if (pageTable[minIndex].dirty == true) {
                        totalWriteToDisks++;
                        System.out.println(parts[0] + " page fault - evict dirty");
                    } else {
                        System.out.println(parts[0] + " page fault - evict clean");
                    }
                    pageTable[minIndex] = temp;
                    recent[minIndex] = (int) Math.pow(2, frames);

                }
                //
                for (j = 0; j < frameCount; j++) {
                    recent[j] = recent[j] / 2;
                }
                totalMemAccesses++;

            }
        } else {
            System.out.println("You have not chosen an algorithm..... STOP TESTING THIS CODE LIKE THAT");
            System.exit(0);
        }
        System.out.println(method);
        System.out.println("Number of frames: " + frames);
        System.out.println("Total memory accesses " + totalMemAccesses);
        System.out.println("Total Page Faults " + totalPageFaults);
        System.out.println("Total write to disks " + totalWriteToDisks);

    }

}
