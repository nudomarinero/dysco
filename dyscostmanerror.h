#ifndef DYNSTACOM_STMAN_ERROR_H
#define DYNSTACOM_STMAN_ERROR_H

#include <casacore/tables/DataMan/DataManError.h>

namespace dyscostman
{

/**
 * Represents a runtime exception that occured within the OffringaStMan.
 */
class DyscoStManError : public casacore::DataManError
{
public:
	/** Constructor */
	DyscoStManError() : casacore::DataManError()
	{	}
	/** Construct with message.
	 * @param message The exception message. */
	DyscoStManError(const std::string& message) : casacore::DataManError(message + " -- Error occured inside the DyscoStManError")
	{ }
};

} // end of namespace

#endif
