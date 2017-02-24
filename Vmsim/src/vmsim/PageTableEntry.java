/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package vmsim;

/**
 *
 * @author Timothy Kang
 */
public class PageTableEntry{
    

    long address;

    boolean dirty;
    boolean referenced;

    public PageTableEntry(){
        
    
        this.address = 0;
        this.dirty = false;
        this.referenced = false;
    }

    public PageTableEntry(PageTableEntry entry){
        
       
        this.address = entry.address;
   
        this.dirty = entry.dirty;
        this.referenced = entry.referenced;
    }
}
