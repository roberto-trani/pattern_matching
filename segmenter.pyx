# distutils: language=c++

from libc.stdint cimport uint16_t, uint32_t, int32_t, uint64_t
from libcpp.vector cimport vector

cimport cython

cimport pattern_matcher
from pattern_matcher cimport PatternMatcher, PatternMatches


cdef class PySegmenter(object):
    cdef PatternMatcher[uint32_t] * c_matcher
    cdef vector[uint16_t] c_segments_lengths
    cdef vector[uint64_t] c_segments_gains
    cdef uint64_t c_num_segments

    def __cinit__(
            self,
            set segments_set,
            dict segment_to_phrase_freq,
            dict segment_to_and_freq,
            double min_segmentation_probability,
            long min_segmentation_freq,
            debug=False
    ):
        # create a filtered list of segments which will be used for the segmentation
        if debug:
            print "Fetching segments to use for the segmentation"
        assert min_segmentation_probability >= 0 or min_segmentation_probability == -1
        assert min_segmentation_freq >= 0
        segment_to_and_freq = dict(
            (" ".join(sorted(segment.split())) if " " in segment else segment, freq)
            for segment, freq in segment_to_and_freq.iteritems()
        )

        if min_segmentation_probability == -1:
            segments_freqs = tuple(
                (segment, phrase_freq)
                for segment, segment_length, (phrase_freq, and_freq) in (
                    (segment, len(segment.split()) if "  " in segment else (segment.count(" ")+1), get_segment_details(segment, segment_to_phrase_freq, segment_to_and_freq))
                    for segment in segments_set
                    if " " in segment  # are we sure we want only multi-terms segments here?
                )
                if phrase_freq >= min_segmentation_freq
                and (segment_length ** segment_length) * phrase_freq >= and_freq
            )
        else:
            segments_freqs = tuple(
                (segment, phrase_freq)
                for segment, (phrase_freq, and_freq) in (
                    (segment, get_segment_details(segment, segment_to_phrase_freq, segment_to_and_freq))
                    for segment in segments_set
                    if " " in segment  # are we sure we want only multi-terms segments here?
                )
                if phrase_freq >= min_segmentation_freq
                and phrase_freq >= (min_segmentation_probability * (and_freq if and_freq > 0 else 1))
            )

        self.c_num_segments = len(segments_freqs)
        self.c_segments_gains = vector[uint64_t](self.c_num_segments)
        self.c_segments_lengths = vector[uint16_t](self.c_num_segments)
        if debug:
            print "Fetched {} segments".format(self.c_num_segments)

        # create the matcher
        self.c_matcher = new PatternMatcher[uint32_t]()

        # add the patterns
        if debug:
            print "Adding patterns into PatternMatcher"

        cdef uint32_t segment_pos
        cdef str segment
        cdef uint64_t phrase_freq
        cdef uint64_t segment_len
        for segment_pos in range(self.c_num_segments):
            segment, phrase_freq = segments_freqs[segment_pos]
            segment_len = len(segment.split())
            self.c_segments_lengths[segment_pos] = segment_len
            self.c_segments_gains[segment_pos] = (segment_len ** segment_len) * phrase_freq
            self.c_matcher.add_pattern(segment_pos, segment)

        # compile the matcher
        if debug:
            print "Compiling the PatternMatcher"
        self.c_matcher.compile()

        if debug:
            print "End PySegmenter.__init__"


    def __dealloc__(self):
        del self.c_matcher


    @cython.boundscheck(False)
    @cython.wraparound(False)
    @cython.nonecheck(False)
    @cython.cdivision(False)
    cdef c_segment(self, str text):
        # terms of the text
        cdef terms = text.split()
        # early exit
        if len(terms) == 0:
            return []

        # find the segments inside the text
        cdef PatternMatches[uint32_t] c_matches = PatternMatches[uint32_t](True)
        self.c_matcher.find_patterns(text, c_matches)
        cdef int32_t num_matches = c_matches.size()
        # early exit
        if num_matches == 0:
            return terms


        # buffer vector will be deallocated at the end of the function
        cdef vector[uint32_t] buffer_vector = vector[uint32_t](num_matches * 6)

        # vector with the start position of each segment
        cdef uint32_t * start_vec = &buffer_vector[num_matches * 0]
        # vector with the end position of each segment
        cdef uint32_t * end_vec = &buffer_vector[num_matches * 1]
        # vector with the (precomputed) gain of each segment
        cdef uint32_t * gain_vec = &buffer_vector[num_matches * 2]
        # vector with the segmentation back references
        cdef int32_t * best_back_pos = <int32_t*>&buffer_vector[num_matches * 3]
        # vector with the best segmentation gains
        cdef uint32_t * best_gain = &buffer_vector[num_matches * 4]
        # vecotor with the positions of the best segments for each position of the vector matches
        cdef int32_t * best_pos = <int32_t*>&buffer_vector[num_matches * 5]

        cdef int32_t pos,
        cdef uint32_t start_pos, end_pos, freq, length
        cdef uint32_t segment_pos
        for pos in range(num_matches):
            segment_pos = c_matches.at(pos).pattern
            end_pos = c_matches.at(pos).end_pos + 1
            length = self.c_segments_lengths[segment_pos]
            start_pos = end_pos - length

            start_vec[pos] = start_pos
            end_vec[pos] = end_pos
            gain_vec[pos] = self.c_segments_gains[segment_pos]

        # SEGMENTATION using dynamic programming
        # value of the gain in the current position
        cdef uint64_t gain = 0

        # variables used in the loop
        cdef int32_t prev_pos
        for pos in range(num_matches):
            start_pos = start_vec[pos]

            gain = 0
            best_back_pos[pos] = -1

            if pos > 0:
                # check only the segments on the left of this one
                # stop to go back when the segment is before the previous one (the list is sorted by right position)
                prev_pos = pos-1
                while prev_pos >= 0 and end_vec[prev_pos] > start_pos:
                    prev_pos -= 1

                if prev_pos >= 0:
                    # now prev_pos is the first segment on the left of this one
                    # best_gain[prev_pos] and best_pos[prev_pos] contain the best gain and position reached in prev_pos
                    gain = best_gain[prev_pos]
                    best_back_pos[pos] = best_pos[prev_pos]

            # add the gain due to this segment
            gain += gain_vec[pos]

            # store the best value and position encountered until now in this position
            if pos > 0 and gain <= best_gain[pos-1]:
                best_gain[pos] = best_gain[pos-1]
                best_pos[pos] = best_pos[pos-1]
            else:
                best_gain[pos] = gain
                best_pos[pos] = pos

        # get the positions of the selected segments
        best_pos_list = []
        pos = best_pos[num_matches-1]  # let's start from the last best position
        while pos != -1:
            best_pos_list.append(pos)
            pos = best_back_pos[pos]
        best_pos_list.reverse()  # transform the list with the position from left to right

        # build the optimal solution
        segmentation = []
        cdef uint32_t prev_end_pos = 0
        for pos in best_pos_list:
            start_pos = start_vec[pos]
            end_pos = end_vec[pos]

            segmentation.extend(terms[prev_end_pos:start_pos])
            segmentation.append(" ".join(terms[start_pos:end_pos]))
            prev_end_pos = end_pos
        segmentation.extend(terms[prev_end_pos:])

        # return the result
        return segmentation

    cdef c_segment_text(self, str text):
        assert("_" not in text)
        return " ".join([
            segm if segm.count(" ") == 0 else "_{}_".format("_".join(segm.split()))
            for segm in self.c_segment(text)
        ])

    def segment(self, text):
        if isinstance(text, str):
            return self.c_segment(text);
        elif isinstance(text, (list,tuple)):
            # this is needed to maintain futhure compatibility
            return [self.c_segment(piece) for piece in text]
        else:
            raise ValueError("text must be a string or a list of strings (related to different rows of the same document)")

    def segment_text(self, text):
        if isinstance(text, str):
            return self.c_segment_text(text);
        elif isinstance(text, (list,tuple)):
            # this is needed to maintain futhure compatibility
            return [self.c_segment_text(piece) for piece in text]
        else:
            raise ValueError("text must be a string or a list of strings (related to different rows of the same document)")


cdef get_segment_details(str segment, dict segment_to_phrase_freq, dict segment_to_and_freq):
    if " " in segment:
        return (segment_to_phrase_freq[segment], segment_to_and_freq[" ".join(sorted(segment.split()))])
    else:
        freq = segment_to_phrase_freq[segment]
        return (freq, freq)
