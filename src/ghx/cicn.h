#ifndef __cicn_h
#define __cicn_h

#ifndef PACKED
#ifdef __GNUC__
#define PACKED __attribute__((__packed__))
#else
#define PACKED
#endif
#endif

typedef void *Ptr;
typedef void **Handle;
typedef u_int32_t Fixed;

struct Rect {
	u_int16_t top PACKED;	/* upper boundary of rectangle */
	u_int16_t left PACKED;	/* left boundary of rectangle */
	u_int16_t bottom PACKED;/* lower boundary of rectangle */
	u_int16_t right PACKED;	/* right boundary of rectangle */
};

typedef struct Rect Rect;

struct RGBColor {
	u_int16_t red PACKED;	/* magnitude of red component */
	u_int16_t green PACKED;	/* magnitude of green component */
	u_int16_t blue PACKED;	/* magnitude of blue component */
};

typedef struct RGBColor RGBColor;

struct ColorSpec {
	u_int16_t value PACKED;	/* index or other value */
	RGBColor rgb PACKED;	/* true color */
};

typedef struct ColorSpec ColorSpec;
typedef ColorSpec *ColorSpecPtr;

typedef ColorSpec CSpecArray[1];

struct ColorTable {
	u_int32_t ctSeed PACKED;	/* unique identifier for table */
	u_int16_t ctFlags PACKED;	/* high bit: 0 = PixMap; 1 = device */
	u_int16_t ctSize PACKED;	/* number of entries in CTTable */
	CSpecArray ctTable PACKED;	/* array [0..0] of ColorSpec */
};

typedef struct ColorTable ColorTable;
typedef ColorTable *CTabPtr, **CTabHandle;

struct PixMap {
	Ptr baseAddr PACKED;		/* pointer to pixels */
	u_int16_t rowBytes PACKED;		/* offset to next line */
	Rect bounds PACKED;		/* encloses bitmap */
	u_int16_t pmVersion PACKED;	/* pixMap version number */
	u_int16_t packType PACKED;		/* defines packing format */
	u_int32_t packSize PACKED;		/* length of pixel data */
	Fixed hRes PACKED;		/* horiz. resolution (ppi) */
	Fixed vRes PACKED;		/* vert. resolution (ppi) */
	u_int16_t pixelType PACKED;	/* defines pixel type */
	u_int16_t pixelSize PACKED;	/* # bits in pixel */
	u_int16_t cmpCount PACKED;		/* # components in pixel */
	u_int16_t cmpSize PACKED;		/* # bits per component */
	u_int32_t planeBytes PACKED;	/* offset to next plane */
	CTabHandle pmTable PACKED;	/* color map for this pixMap */
	u_int32_t pmReserved PACKED;	/* for future use. MUST BE 0 */
};

typedef struct PixMap PixMap;
typedef PixMap *PixMapPtr, **PixMapHandle;

struct BitMap {
	Ptr baseAddr PACKED;
	u_int16_t rowBytes PACKED;
	Rect bounds PACKED;
};

typedef struct BitMap BitMap;
typedef BitMap *BitMapPtr, **BitMapHandle;

struct CIcon {
	PixMap iconPMap PACKED;	/* the icon's pixMap */
	BitMap iconMask PACKED;	/* the icon's mask */
	BitMap iconBMap PACKED;	/* the icon's bitMap */
	Handle iconData PACKED;	/* the icon's data */
	u_int16_t iconMaskData[1] PACKED;/* icon's mask and BitMap data */
};

typedef struct CIcon CIcon;
typedef CIcon *CIconPtr, **CIconHandle;

/* 
struct cicn_resource {
	icon's pixel map;		// 50 bytes
	mask bitmap;			// 14
	icon's bitmap;			// 14
	icon's data;			// 4
	mask bitmap image data;		// variable
	icon's bitmap image data;	// variable
	icon's color table;		// variable
	pixel map image data;		// variable
};
*/
#endif /* __cicn_h */
