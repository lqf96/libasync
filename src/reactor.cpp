#include <string.h>
#include <libasync/reactor.h>

namespace libasync
{   //Reactor exception constructor
    ReactorError::ReactorError(Reason __reason, int __error_num)
        : _reason(__reason), _error_num(__error_num) {}

    //Get reason
    ReactorError::Reason ReactorError::reason() const noexcept
    {   return this->_reason;
    }

    //Get error number
    int ReactorError::error_num() const noexcept
    {   return this->_error_num;
    }

    //Get error information
    const char* ReactorError::what() const noexcept
    {   return strerror(this->_error_num);
    }
}
