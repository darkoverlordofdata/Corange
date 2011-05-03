#include <stdio.h>
#include <string.h>
#include <math.h>

#define GLEW_STATIC
#include "GL/glew.h"

#define NO_SDL_GLEXT
#include "SDL.h"
#include "SDL_opengl.h"

#include "asset_manager.h"
#include "texture.h"

#include "dds_loader.h"

DdsLoadInfo loadInfoDXT1 = {
  1, 0, 0, 4, 8, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
};
DdsLoadInfo loadInfoDXT3 = {
  1, 0, 0, 4, 16, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
};
DdsLoadInfo loadInfoDXT5 = {
  1, 0, 0, 4, 16, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
};
DdsLoadInfo loadInfoBGRA8 = {
  0, 0, 0, 1, 4, GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE
};
DdsLoadInfo loadInfoBGR8 = {
  0, 0, 0, 1, 3, GL_RGB8, GL_BGR, GL_UNSIGNED_BYTE
};
DdsLoadInfo loadInfoBGR5A1 = {
  0, 1, 0, 1, 2, GL_RGB5_A1, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV
};
DdsLoadInfo loadInfoBGR565 = {
  0, 1, 0, 1, 2, GL_RGB5, GL_RGB, GL_UNSIGNED_SHORT_5_6_5
};
DdsLoadInfo loadInfoIndex8 = {
  0, 0, 1, 1, 1, GL_RGB8, GL_BGRA, GL_UNSIGNED_BYTE
};

#define max(A,B) (((A)>(B))?(A):(B))

texture* dds_load_file( char* filename ){
  
  printf("Loading File: %s\n", filename);
  
  texture my_texture;
  glGenTextures(1, &my_texture);
  glBindTexture(GL_TEXTURE_2D, my_texture);

  DDS_header hdr;
  size_t s = 0;
  int x = 0;
  int y = 0;
  int mipMapCount = 0;
  
  SDL_RWops* f = SDL_RWFromFile(filename, "rb");
  SDL_RWread(f, &hdr, 1, sizeof(hdr));
  
  if( hdr.dwMagic != DDS_MAGIC || hdr.dwSize != 124 ||
    !(hdr.dwFlags & DDSD_PIXELFORMAT) || !(hdr.dwFlags & DDSD_CAPS) ) {
    
    printf("Error Loading File %s: Does not appear to be a .dds file.\n", filename);
    return NULL;
  }

  x = hdr.dwWidth;
  y = hdr.dwHeight;

  DdsLoadInfo* li;

  if( PF_IS_DXT1( hdr.sPixelFormat ) ) {
    li = &loadInfoDXT1;
  }
  else if( PF_IS_DXT3( hdr.sPixelFormat ) ) {
    li = &loadInfoDXT3;
  }
  else if( PF_IS_DXT5( hdr.sPixelFormat ) ) {
    li = &loadInfoDXT5;
  }
  else if( PF_IS_BGRA8( hdr.sPixelFormat ) ) {
    li = &loadInfoBGRA8;
  }
  else if( PF_IS_BGR8( hdr.sPixelFormat ) ) {
    li = &loadInfoBGR8;
  }
  else if( PF_IS_BGR5A1( hdr.sPixelFormat ) ) {
    li = &loadInfoBGR5A1;
  }
  else if( PF_IS_BGR565( hdr.sPixelFormat ) ) {
    li = &loadInfoBGR565;
  }
  else if( PF_IS_INDEX8( hdr.sPixelFormat ) ) {
    li = &loadInfoIndex8;
  }
  else {
    printf("Error Loading File %s: Unknown DDS File format type.\n", filename);
    return NULL;
  }
  
  glTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE );
  mipMapCount = (hdr.dwFlags & DDSD_MIPMAPCOUNT) ? hdr.dwMipMapCount : 1;
  
  int ix, zz;
  GLenum cFormat, format;
  
  if( li->compressed ) {
    size_t size = max( li->divSize, x )/li->divSize * max( li->divSize, y )/li->divSize * li->blockBytes;
    char* data = malloc( size );
    
    if( !data ) {
      printf("Error Loading File %s: Does not appear to contain any data.\n", filename);
      return NULL;
    }
    
    cFormat = li->internalFormat;
    format = li->internalFormat;
    
    for( ix = 0; ix < mipMapCount; ++ix ) {
    
      SDL_RWread(f, data, 1, size);
      glCompressedTexImage2D( GL_TEXTURE_2D, ix, li->internalFormat, x, y, 0, size, data );
      
      x = (x+1)>>1;
      y = (y+1)>>1;
      
      size = max( li->divSize, x )/li->divSize * max( li->divSize, y )/li->divSize * li->blockBytes;
    }
    free( data );
    
  } else if( li->palette ) {
  
    size_t size = hdr.dwPitchOrLinearSize * y;
    format = li->externalFormat;
    cFormat = li->internalFormat;
    char* data = malloc( size );
    int palette[256];
    int* unpacked = malloc( size * sizeof(int) );
    
    SDL_RWread(f, palette, 4, 256);
    for( ix = 0; ix < mipMapCount; ++ix ) {
    
      SDL_RWread(f, data, 1, size);
      
      for( zz = 0; zz < size; ++zz ) {
        unpacked[ zz ] = palette[ data[ zz ] ];
      }
      
      glPixelStorei( GL_UNPACK_ROW_LENGTH, y );
      glTexImage2D( GL_TEXTURE_2D, ix, li->internalFormat, x, y, 0, li->externalFormat, li->type, unpacked );
      
      x = (x+1)>>1;
      y = (y+1)>>1;
      
      size = x * y * li->blockBytes;
    }
    free( data );
    free( unpacked );
    
  } else {
  
    if( li->swap ) {
      glPixelStorei( GL_UNPACK_SWAP_BYTES, GL_TRUE );
    }
    
    size_t size = x * y * li->blockBytes;
    format = li->externalFormat;
    cFormat = li->internalFormat;
    char * data = malloc( size );
    //fixme: how are MIP maps stored for 24-bit if pitch != ySize*3 ?
    for( ix = 0; ix < mipMapCount; ++ix ) {
    
      SDL_RWread(f, data, 1, size);
      
      glPixelStorei( GL_UNPACK_ROW_LENGTH, y );
      glTexImage2D( GL_TEXTURE_2D, ix, li->internalFormat, x, y, 0, li->externalFormat, li->type, data );
      
      x = (x+1)>>1;
      y = (y+1)>>1;
      size = x * y * li->blockBytes;
    }
    free( data );
    glPixelStorei( GL_UNPACK_SWAP_BYTES, GL_FALSE );
  }
  
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipMapCount-1 );
  
  texture* tex = malloc(sizeof(texture));
  *tex = my_texture;
  
  return tex;
  
}
