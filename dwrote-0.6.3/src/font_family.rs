/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::cell::UnsafeCell;

use comptr::ComPtr;
use winapi::um::dwrite::{IDWriteFont, IDWriteFontFamily, IDWriteFontCollection};
use winapi::um::dwrite::{IDWriteLocalizedStrings};

use super::*;
use helpers::*;

pub struct FontFamily {
    native: UnsafeCell<ComPtr<IDWriteFontFamily>>,
}

impl FontFamily {
    pub fn take(native: ComPtr<IDWriteFontFamily>) -> FontFamily {
        FontFamily {
            native: UnsafeCell::new(native)
        }
    }

    pub unsafe fn as_ptr(&self) -> *mut IDWriteFontFamily {
        (*self.native.get()).as_ptr()
    }

    pub fn name(&self) -> String {
        unsafe {
            let mut family_names: ComPtr<IDWriteLocalizedStrings> = ComPtr::new();
            let hr = (*self.native.get()).GetFamilyNames(family_names.getter_addrefs());
            assert!(hr == 0);

            get_locale_string(&mut family_names)
        }
    }

    pub fn get_first_matching_font(&self,
                                   weight: FontWeight,
                                   stretch: FontStretch,
                                   style: FontStyle)
        -> Font
    {
        unsafe {
            let mut font: ComPtr<IDWriteFont> = ComPtr::new();
            let hr = (*self.native.get()).GetFirstMatchingFont(weight.t(), stretch.t(), style.t(), font.getter_addrefs());
            assert!(hr == 0);
            Font::take(font)
        }
    }

    pub fn get_font_collection(&self) -> FontCollection {
        unsafe {
            let mut collection: ComPtr<IDWriteFontCollection> = ComPtr::new();
            let hr = (*self.native.get()).GetFontCollection(collection.getter_addrefs());
            assert!(hr == 0);
            FontCollection::take(collection)
        }
    }

    pub fn get_font_count(&self) -> u32 {
        unsafe {
            (*self.native.get()).GetFontCount()
        }
    }

    pub fn get_font(&self, index: u32) -> Font {
        unsafe {
            let mut font: ComPtr<IDWriteFont> = ComPtr::new();
            let hr = (*self.native.get()).GetFont(index, font.getter_addrefs());
            assert!(hr == 0);
            assert!(!font.as_ptr().is_null());
            Font::take(font)
        }
    }
}
