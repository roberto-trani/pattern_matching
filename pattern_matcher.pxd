from libc.stdint cimport uint32_t
from libcpp cimport bool
from libcpp.string cimport string
from libcpp.unordered_map cimport unordered_map
from libcpp.unordered_set cimport unordered_set

ctypedef unsigned short ushort


cdef extern from "AhoCorasickAutomaton.hpp":
    cdef cppclass PatternMatch[T]:
        const T     pattern
        size_t      end_pos
        PatternMatch(const T &, size_t)

    cdef cppclass PatternMatches[T]:
        PatternMatches()
        PatternMatches(bool)
        bool                        include_suffixes() const
        void                        clear()
        void                        reserve(size_t)
        size_t                      size()
        void                        push_back(PatternMatch[T]&)
        PatternMatch[T]&            operator[](size_t)
        PatternMatch[T]&            at(size_t)


cdef extern from "PatternMatcher.hpp":
    cdef cppclass PatternMatcher[T]:
        PatternMatcher()
        void                                    add_pattern(T, const string &) except +
        void                                    compile() except +
        void                                    complete_with_suffix_matches(PatternMatches[T] &, PatternMatches[T] &) except +
        void                                    find_patterns(const string &, PatternMatches[T] &) except +
        ushort                                  get_pattern_length(T) except +
        const unordered_map[T, ushort] &        get_pattern_length_map()
        const unordered_set[ushort] &           get_pattern_set()
        void                                    reserve(size_t)


cdef class PyPatternMatches:
    cdef PatternMatches[uint32_t] * c_matches


cdef class PyPatternMatcher:
    cdef PatternMatcher[uint32_t] * c_matcher
