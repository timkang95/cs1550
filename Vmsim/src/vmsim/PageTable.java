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
public class PageTable{
    //which index the page possess (capped by maximum from argument input)
    int index;
    //properties of page
    int frame;
    boolean valid;
    boolean dirty;
    boolean referenced;

    public PageTable(){
        //initialize to arbitarary values
        this.index = 0;
        this.frame = -1;
        this.valid = false;
        this.dirty = false;
        this.referenced = false;
    }

    public PageTable(PageTable page){
        this.index = page.index;
        this.frame = page.frame;
        this.valid = page.valid;
        this.dirty = page.dirty;
        this.referenced = page.referenced;
    }
}
