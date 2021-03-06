/*
 * Copyright © 2011,2012  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_OT_NAME_TABLE_HH
#define HB_OT_NAME_TABLE_HH

#include "hb-open-type.hh"
#include "hb-ot-name-language.hh"
#include "hb-aat-layout.hh"


namespace OT {


#define entry_score var.u16[0]
#define entry_index var.u16[1]


/*
 * name -- Naming
 * https://docs.microsoft.com/en-us/typography/opentype/spec/name
 */
#define HB_OT_TAG_name HB_TAG('n','a','m','e')

#define UNSUPPORTED	42

struct NameRecord
{
  inline hb_language_t language (hb_face_t *face) const
  {
    unsigned int p = platformID;
    unsigned int l = languageID;

    if (p == 3)
      return _hb_ot_name_language_for_ms_code (l);

    if (p == 1)
      return _hb_ot_name_language_for_mac_code (l);

    if (p == 0)
      return _hb_aat_language_get (face, l);

    return HB_LANGUAGE_INVALID;
  }

  inline uint16_t score (void) const
  {
    /* Same order as in cmap::find_best_subtable(). */
    unsigned int p = platformID;
    unsigned int e = encodingID;

    /* 32-bit. */
    if (p == 3 && e == 10) return 0;
    if (p == 0 && e ==  6) return 1;
    if (p == 0 && e ==  4) return 2;

    /* 16-bit. */
    if (p == 3 && e ==  1) return 3;
    if (p == 0 && e ==  3) return 4;
    if (p == 0 && e ==  2) return 5;
    if (p == 0 && e ==  1) return 6;
    if (p == 0 && e ==  0) return 7;

    /* Symbol. */
    if (p == 3 && e ==  0) return 8;

    /* We treat all Mac Latin names as ASCII only. */
    if (p == 1 && e ==  0) return 10; /* 10 is magic number :| */

    return UNSUPPORTED;
  }

  inline bool sanitize (hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    /* We can check from base all the way up to the end of string... */
    return_trace (c->check_struct (this) && c->check_range ((char *) base, (unsigned int) length + offset));
  }

  HBUINT16	platformID;	/* Platform ID. */
  HBUINT16	encodingID;	/* Platform-specific encoding ID. */
  HBUINT16	languageID;	/* Language ID. */
  HBUINT16	nameID;		/* Name ID. */
  HBUINT16	length;		/* String length (in bytes). */
  HBUINT16	offset;		/* String offset from start of storage area (in bytes). */
  public:
  DEFINE_SIZE_STATIC (12);
};

static int
_hb_ot_name_entry_cmp_key (const void *pa, const void *pb)
{
  const hb_ot_name_entry_t *a = (const hb_ot_name_entry_t *) pa;
  const hb_ot_name_entry_t *b = (const hb_ot_name_entry_t *) pb;

  /* Compare by name_id, then language. */

  if (a->name_id != b->name_id)
    return a->name_id < b->name_id ? -1 : +1;

  if (a->language == b->language) return 0;
  if (!a->language) return -1;
  if (!b->language) return +1;
  return strcmp (hb_language_to_string (a->language),
		 hb_language_to_string (b->language));
}

static int
_hb_ot_name_entry_cmp (const void *pa, const void *pb)
{
  /* Compare by name_id, then language, then score, then index. */

  int v = _hb_ot_name_entry_cmp_key (pa, pb);
  if (v)
    return v;

  const hb_ot_name_entry_t *a = (const hb_ot_name_entry_t *) pa;
  const hb_ot_name_entry_t *b = (const hb_ot_name_entry_t *) pb;

  if (a->entry_score != b->entry_score)
    return a->entry_score < b->entry_score ? -1 : +1;

  if (a->entry_index != b->entry_index)
    return a->entry_index < b->entry_index ? -1 : +1;

  return 0;
}

struct name
{
  static const hb_tag_t tableTag	= HB_OT_TAG_name;

  inline unsigned int get_size (void) const
  { return min_size + count * nameRecordZ[0].min_size; }

  inline bool sanitize_records (hb_sanitize_context_t *c) const {
    TRACE_SANITIZE (this);
    const void *string_pool = (this+stringOffset).arrayZ;
    unsigned int _count = count;
    /* Move to run-time?! */
    for (unsigned int i = 0; i < _count; i++)
      if (!nameRecordZ[i].sanitize (c, string_pool)) return_trace (false);
    return_trace (true);
  }

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  likely (format == 0 || format == 1) &&
		  c->check_array (nameRecordZ.arrayZ, count) &&
		  c->check_range (this, stringOffset));
  }

  struct accelerator_t
  {
    inline void init (hb_face_t *face)
    {
      this->blob = hb_sanitize_context_t().reference_table<name> (face);
      this->table = this->blob->as<name> ();
      assert (this->blob->length >= this->table->stringOffset);
      this->pool = (this->table+this->table->stringOffset).arrayZ;
      this->pool_len = this->blob->length - this->table->stringOffset;
      const hb_array_t<const NameRecord> all_names (this->table->nameRecordZ.arrayZ,
						    this->table->count);

      this->names.init ();
      this->names.alloc (all_names.len);

      for (uint16_t i = 0; i < all_names.len; i++)
      {
	hb_ot_name_entry_t *entry = this->names.push ();

	entry->name_id = all_names[i].nameID;
	entry->language = all_names[i].language (face);
	entry->entry_score =  all_names[i].score ();
	entry->entry_index = i;
      }

      this->names.qsort (_hb_ot_name_entry_cmp);
      /* Walk and pick best only for each name_id,language pair,
       * while dropping unsupported encodings. */
      unsigned int j = 0;
      for (unsigned int i = 0; i < this->names.len; i++)
      {
        if (this->names[i].entry_score == UNSUPPORTED ||
	    this->names[i].language == HB_LANGUAGE_INVALID)
	  continue;
        if (i &&
	    this->names[i - 1].name_id  == this->names[i].name_id &&
	    this->names[i - 1].language == this->names[i].language)
	  continue;
	this->names[j++] = this->names[i];
      }
      this->names.resize (j);
    }

    inline void fini (void)
    {
      this->names.fini ();
      hb_blob_destroy (this->blob);
    }

    inline int get_index (hb_ot_name_id_t   name_id,
			  hb_language_t     language,
			  unsigned int     *width=nullptr) const
    {
      const hb_ot_name_entry_t key = {name_id, {0}, language};
      const hb_ot_name_entry_t *entry = (const hb_ot_name_entry_t *)
					hb_bsearch (&key,
						    this->names.arrayZ(),
						    this->names.len,
						    sizeof (key),
						    _hb_ot_name_entry_cmp_key);
      if (!entry)
        return -1;

      if (width)
        *width = entry->entry_score < 10 ? 2 : 1;

      return entry->entry_index;
    }

    inline hb_bytes_t get_name (unsigned int idx) const
    {
      const hb_array_t<const NameRecord> all_names (table->nameRecordZ.arrayZ, table->count);
      const NameRecord &record = all_names[idx];
      const hb_array_t<const char> string_pool ((const char *) pool, pool_len);
      return string_pool.sub_array (record.offset, record.length).as_bytes ();
    }

    private:
    hb_blob_t *blob;
    const void *pool;
    unsigned int pool_len;
    public:
    hb_nonnull_ptr_t<const name> table;
    hb_vector_t<hb_ot_name_entry_t> names;
  };

  /* We only implement format 0 for now. */
  HBUINT16	format;			/* Format selector (=0/1). */
  HBUINT16	count;			/* Number of name records. */
  OffsetTo<UnsizedArrayOf<HBUINT8>, HBUINT16, false>
		stringOffset;		/* Offset to start of string storage (from start of table). */
  UnsizedArrayOf<NameRecord>
		nameRecordZ;		/* The name records where count is the number of records. */
  public:
  DEFINE_SIZE_ARRAY (6, nameRecordZ);
};

struct name_accelerator_t : name::accelerator_t {};

} /* namespace OT */


#endif /* HB_OT_NAME_TABLE_HH */
