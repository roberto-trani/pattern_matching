#ifndef BUFFERMANAGER_HPP
#define BUFFERMANAGER_HPP

#include <iostream>


template<typename _Tp>
class DataBlock {
private:
    const _Tp * _data;
    size_t _size;

public:
    DataBlock(const _Tp * data, size_t size)
            : _data(data), _size(size) {
    }

    DataBlock<_Tp>
    sub(size_t index, size_t size) const {
        return DataBlock<_Tp>(this->_data + index, size);
    }

    size_t
    find(_Tp delimiter, size_t index=0) const {
        for (; index < this->_size; ++index) {
            if (this->_data[index] == delimiter)
                return index;
        }
        return this->_size;
    }

    const _Tp *
    data() const {
        return this->_data;
    }

    size_t
    size() const {
        return this->_size;
    }

    bool
    operator==(const DataBlock<_Tp>& db) const {
        return (this->_size == db._size) &&
               (memcmp (this->_data, db._data, this->_size) == 0);
    }

    friend std::ostream&
    operator<<(std::ostream& o_stream, const DataBlock<_Tp>& db) {
        for (size_t i=0, max_i=db.size(); i < max_i; ++i) {
            o_stream << db._data[i];
        }
        return o_stream;
    }

    DataBlock<_Tp>&
    operator=(const DataBlock<_Tp>& db) {
        this->_data = db._data;
        this->_size = db._size;
        return *this;
    }
};

// extend the hash implementation to DataBlock
namespace std {
    template<typename _Tp>
    struct hash<DataBlock<_Tp>> : public __hash_base<size_t, DataBlock<_Tp>> {
        size_t
        operator() ( const DataBlock<_Tp> & db ) const noexcept {
            return _Hash_impl::hash ( db.data(), db.size() * sizeof(_Tp) );
        }
    };
}


// Container of the DataBlock buffers used to manage the memory
class BufferManager {
private:
    static const size_t POINTER_SIZE = sizeof(void *);
    const size_t buffer_size;
    void * last_buffer_pointer = 0;
    void * last_buffer_write_pointer = 0;
    size_t last_buffer_space = 0;

public:
    BufferManager(size_t buffer_size = 64 * 1024 * 1024)
            : buffer_size(buffer_size) {
    }

    ~BufferManager() {
        // follow the back link in the first bytes of each block and frees the space
        while (this->last_buffer_pointer != 0) {
            char * pointer = (char *) this->last_buffer_pointer;
            this->last_buffer_pointer = ((void **) this->last_buffer_pointer)[0];
            delete[] pointer;
        }
    }

    template <typename _Tp>
    DataBlock<_Tp> createDataBlock(const _Tp * source, size_t size) {
        size_t space_required = size * sizeof (_Tp);

        // check if there is enough space
        if (last_buffer_space < space_required) {
            size_t buffer_size = this->buffer_size;
            // check if the default buffer size is enough for this copy
            if (space_required > this->buffer_size)
                buffer_size = space_required;

            // allocate the buffer
            void * new_buffer = new char[POINTER_SIZE + buffer_size];
            // save the pointer to the previous buffer in the first bytes of this block
            *((void **) new_buffer) = this->last_buffer_pointer;
            // update the internal state
            this->last_buffer_pointer = new_buffer;
            this->last_buffer_write_pointer = (char *) new_buffer + POINTER_SIZE;
            this->last_buffer_space = this->buffer_size;
        }

        // copy the source into the buffer
        memcpy (this->last_buffer_write_pointer, source, space_required);

        // save the result
        DataBlock<_Tp> result(static_cast<_Tp *>(this->last_buffer_write_pointer), size);
        // update the internal state
        this->last_buffer_write_pointer = (char *) this->last_buffer_write_pointer + space_required;
        this->last_buffer_space -= space_required;

        // return the copied space location
        return result;
    }
};


// define MyString as a DataBlock
typedef DataBlock<char> MyString;


#endif //BUFFERMANAGER_HPP
