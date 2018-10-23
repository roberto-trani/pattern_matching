# distutils: language=c++

import struct
from cython.operator cimport dereference


cdef class PyPatternMatches:
    def __cinit__(self, bool include_suffixes):
        self.c_matches = new PatternMatches[uint32_t](include_suffixes)

    def __dealloc__(self):
        del self.c_matches

    def clear(self):
        self.c_matches.clear()

    def size(self):
        return self.c_matches.size()

    def __iter__(self):
        cdef int i
        for i in range(self.size()):
            yield self.__getitem__(i)

    def __getitem__(self, size_t pos):
        return (self.c_matches.at(pos).pattern, self.c_matches.at(pos).end_pos)

    def get_pattern_id(self, size_t pos):
        return self.c_matches.at(pos).pattern

    def get_end_pos(self, size_t pos):
        return self.c_matches.at(pos).end_pos

    def dumps(self):
        return "".join(struct.pack("<2I", pattern, end_pos) for (pattern, end_pos) in self)

    def loads(self, binary_data):
        cdef uint32_t pattern
        cdef size_t end_pos
        for i in range(0, len(binary_data), 8):
            pattern, end_pos = struct.unpack('<2I', binary_data[i:i+8])
            self.c_matches.push_back(PatternMatch[uint32_t](pattern, end_pos))


cdef class PyPatternMatcher:
    def __cinit__(self):
        self.c_matcher = new PatternMatcher[uint32_t]()

    def __dealloc__(self):
        del self.c_matcher

    def add_pattern(self, uint32_t pattern_id, string pattern):
        self.c_matcher.add_pattern(pattern_id, pattern)

    def compile(self):
        self.c_matcher.compile()

    def complete_with_suffix_matches(self, PyPatternMatches src_matches, PyPatternMatches dst_matches):
        self.c_matcher.complete_with_suffix_matches(dereference(src_matches.c_matches), dereference(dst_matches.c_matches))

    def find_patterns(self, str text, PyPatternMatches matches):
        self.c_matcher.find_patterns(text, dereference(matches.c_matches))

    def get_pattern_length(self, uint32_t pattern_id):
        return self.c_matcher.get_pattern_length(pattern_id)

    def reserve(self, size_t num_patterns):
        self.c_matcher.reserve(num_patterns)
