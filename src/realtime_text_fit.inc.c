u8 GetFontSmallLatinGlyphWidth(u16 glyphId);
const u16 *GetFontSmallLatinGlyphData(u16 glyphId);
u8 GetFontHalfRowOffset(u8 halfRow);
// Render complete native glyphs into a measured rectangle. No character is
// discarded to fit a HUD label, and no signed centering can underflow a window.
static void RtPrintFit(u8 window,u8 font,s16 x,s16 y,u16 width,u16 height,const u8 *colors,const u8 *text)
{
 const u8 *read=text;u16 line=0,lines=1,maxWidth=0,total=0,pen=0,scaleX,scaleY,inkTop=0,inkBottom,sourceHeight;u8 c,oldFg,oldBg,oldShadow;bool8 compact=TRUE;
 u8 *pixels=(u8 *)GetWindowAttribute(sRt->winIds[window],WINDOW_TILE_DATA);u16 tiles=GetWindowAttribute(sRt->winIds[window],WINDOW_WIDTH),wh=GetWindowAttribute(sRt->winIds[window],WINDOW_HEIGHT)*8;
 if(x<0)x=0;if(y<0)y=0;if(x>=tiles*8 || y>=wh || !width || !height)return;width=min(width,tiles*8-x);height=min(height,wh-y);
 while((c=*read++)!=EOS)
 {
  if(c==EXT_CTRL_CODE_BEGIN){read+=GetExtCtrlCodeLength(*read);compact=FALSE;continue;}
  if(c==CHAR_NEWLINE || c==CHAR_PROMPT_SCROLL || c==CHAR_PROMPT_CLEAR){if(total>maxWidth)maxWidth=total;total=0;lines++;continue;}
  // Small-font letters, digits and most punctuation have ink only in glyph rows 4-11.
  if(c!=CHAR_SPACE && (c<0xA1 || c>0xEE || c==0xB1 || c==0xB2 || c==0xB3 || c==0xB4 || c==0xB8 || c==0xDB || c==0xE4 || c==0xE5 || c==0xED))compact=FALSE;
  total+=GetFontSmallLatinGlyphWidth(c);
 }
 if(total>maxWidth)maxWidth=total;if(!maxWidth)return;
 scaleX=max(1,maxWidth>width?width*256/maxWidth:256);sourceHeight=lines*13;
 if(scaleX==256 && height>=sourceHeight){AddTextPrinterParameterized3(sRt->winIds[window],FONT_SMALL,x,y,colors,TEXT_SKIP_DRAW,text);return;}
 // Fast path for one short HUD label: blit glyph rows 4-11 (where small-font
 // letters and digits have their ink) straight into the window, one pass.
 if(scaleX==256 && lines==1 && compact && height>=8)
 {
  u16 yy,xx,gx,gy,offset,row;u32 cls;u8 value;const u16 *glyph;
  // Pixel classes per glyph half-row byte (2 bits each: 0 empty, 1 ink, 2 shadow),
  // so no shared font colour table has to be rebuilt for every label.
  if(!sRt->halfRowReady)
  {
   u16 b;u8 idx;
   for(b=0;b<256;b++){idx=GetFontHalfRowOffset(b);sRt->halfRowClass[b]=(idx/27%3)|((idx/9%3)<<2)|((idx/3%3)<<4)|((idx%3)<<6);}
   sRt->halfRowReady=TRUE;
  }
  read=text;
  while((c=*read++)!=EOS)
  {
   u8 gw=GetFontSmallLatinGlyphWidth(c);
   glyph=GetFontSmallLatinGlyphData(c);
   for(yy=4;yy<12;yy++)
   {
    row=glyph[yy];cls=sRt->halfRowClass[row>>8]|(sRt->halfRowClass[row&0xFF]<<8);gy=y+yy-4;
    for(xx=0;xx<gw;xx++,cls>>=2)
    {
     if(!(cls&3))continue;
     value=(cls&3)==1?colors[1]:colors[2];
     gx=x+pen+xx;if(gx>=x+width)break;
     offset=((gy/8)*tiles+gx/8)*32+(gy&7)*4+(gx&7)/2;
     if(gx&1)pixels[offset]=(pixels[offset]&15)|(value<<4);else pixels[offset]=(pixels[offset]&0xF0)|value;
    }
   }
   pen+=gw;
  }
  return;
 }
 SaveTextColors(&oldFg,&oldBg,&oldShadow);GenerateFontHalfRowLookupTable(colors[1],colors[0],colors[2]);
 // Native small glyphs have four empty leading rows. Crop empty padding;
 // never sample every second row of the letters to fit the HUD.
 if(height<sourceHeight)
 {
  u16 xx,yy;inkTop=sourceHeight;inkBottom=0;read=text;line=0;
  while((c=*read++)!=EOS)
  {
   if(c==EXT_CTRL_CODE_BEGIN){read+=GetExtCtrlCodeLength(*read);continue;}
   if(c==CHAR_NEWLINE || c==CHAR_PROMPT_SCROLL || c==CHAR_PROMPT_CLEAR){line++;continue;}
   DecompressGlyph_Small(c,FALSE);
   for(yy=0;yy<13;yy++)for(xx=0;xx<gGlyphInfo.width;xx++)
   {
    u8 value=gGlyphInfo.pixels[(yy/8)*64+(xx/8)*32+(yy&7)*4+(xx&7)/2];value=(value>>((xx&1)*4))&15;
    if(value!=colors[0]){inkTop=min(inkTop,line*13+yy);inkBottom=max(inkBottom,line*13+yy);}
   }
  }
  if(inkTop>inkBottom){RestoreTextColors(&oldFg,&oldBg,&oldShadow);return;}
  sourceHeight=inkBottom-inkTop+1;
 }
 scaleY=max(1,sourceHeight>height?height*256/sourceHeight:256);read=text;line=0;
 while((c=*read++)!=EOS)
 {
  u16 xx,yy,gx,gy,offset,sourceY;u8 value,previous;
  if(c==EXT_CTRL_CODE_BEGIN){read+=GetExtCtrlCodeLength(*read);continue;}
  if(c==CHAR_NEWLINE || c==CHAR_PROMPT_SCROLL || c==CHAR_PROMPT_CLEAR){line++;pen=0;continue;}
  DecompressGlyph_Small(c,FALSE);
  // Visit every source ink pixel. If an unusually long label must shrink,
  // foreground pixels take precedence over shadows at a shared destination.
  for(yy=0;yy<13;yy++)for(xx=0;xx<gGlyphInfo.width;xx++)
  {
   sourceY=line*13+yy;if(sourceY<inkTop || sourceY>=inkTop+sourceHeight)continue;
   value=gGlyphInfo.pixels[(yy/8)*64+(xx/8)*32+(yy&7)*4+(xx&7)/2];value=(value>>((xx&1)*4))&15;if(value==colors[0])continue;
   gx=x+((pen+xx)*scaleX>>8);gy=y+((sourceY-inkTop)*scaleY>>8);if(gx>=x+width || gy>=y+height)continue;
   offset=((gy/8)*tiles+gx/8)*32+(gy&7)*4+(gx&7)/2;previous=(pixels[offset]>>((gx&1)*4))&15;
   if(value==colors[2] && previous==colors[1])continue;
   if(gx&1)pixels[offset]=(pixels[offset]&15)|(value<<4);else pixels[offset]=(pixels[offset]&0xF0)|value;
  }
  pen+=gGlyphInfo.width;
 }
 RestoreTextColors(&oldFg,&oldBg,&oldShadow);
}
static const u8 *RtTextPage(const u8 *source,u8 *page)
{
    u16 at=0,lastSpace=0,width=0;u8 lines=1,c;const u8 *afterSpace=NULL;
    while((c=*source)!=EOS)
    {
        u8 charWidth;
        if(c==EXT_CTRL_CODE_BEGIN){source++;source+=GetExtCtrlCodeLength(*source);continue;}
        if(c==CHAR_NEWLINE || c==CHAR_PROMPT_SCROLL || c==CHAR_PROMPT_CLEAR)
        {source++;if(lines==2)break;page[at++]=CHAR_NEWLINE;lines++;width=0;afterSpace=NULL;continue;}
        DecompressGlyph_Small(c,FALSE);charWidth=gGlyphInfo.width;
        if(width+charWidth>198 || at>=RT_MSG_LEN-3)
        {
            if(afterSpace){at=lastSpace;source=afterSpace;}
            if(lines==2 || at>=RT_MSG_LEN-3)break;
            page[at++]=CHAR_NEWLINE;lines++;width=0;afterSpace=NULL;continue;
        }
        page[at++]=c;source++;width+=charWidth;if(c==CHAR_SPACE){lastSpace=at-1;afterSpace=source;}
    }
    page[at]=EOS;return source;
}

// Transient messages may need more than one two-line result page. Wrap the
// entire message before fitting its rectangle, retaining every character.
static const u8 *RtWrapMessage(const u8 *source,u8 *out)
{
 const u8 *read=source;u8 page[RT_MSG_LEN];u16 at=0,i;
 do {
  read=RtTextPage(read,page);
  for(i=0;page[i]!=EOS;i++){if(at>=RT_MSG_LEN*2-2)return source;out[at++]=page[i];}
  if(*read!=EOS)out[at++]=CHAR_NEWLINE;
 }while(*read!=EOS);
 out[at]=EOS;return out;
}
