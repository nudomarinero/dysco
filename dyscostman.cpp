#include "dyscostman.h"
#include "dyscostmancol.h"
#include "dyscostmanerror.h"
#include "dyscodatacolumn.h"
#include "dyscoweightcolumn.h"

using namespace altthread;

void register_dyscostman()
{
	dyscostman::DyscoStMan::registerClass();
}

namespace dyscostman
{

const unsigned short
	DyscoStMan::VERSION_MAJOR = 1,
	DyscoStMan::VERSION_MINOR = 0;

DyscoStMan::DyscoStMan(unsigned floatBitCount, unsigned weightBitCount, const casacore::String& name) :
	DataManager(),
	_nRow(0),
	_nBlocksInFile(0),
	_rowsPerBlock(0),
	_antennaCount(0),
	_blockSize(0),
	_headerSize(0),
	_name(name),
	_floatBitCount(floatBitCount),
	_weightBitCount(weightBitCount),
	_fitToMaximum(false),
	_distribution(GaussianDistribution),
	_normalization(RFNormalization),
	_studentTNu(0.0),
	_distributionTruncation(0.0),
	_spec()
{
	std::cout << "DyscoStMan(floatBitCount,weightBitCount,...,name)\n";
	initializeSpec();
}

DyscoStMan::DyscoStMan(const casacore::String& name, const casacore::Record& spec) :
	DataManager(),
	_nRow(0),
	_nBlocksInFile(0),
	_rowsPerBlock(0),
	_antennaCount(0),
	_blockSize(0),
	_headerSize(0),
	_name(name),
	_floatBitCount(0),
	_weightBitCount(0),
	_fitToMaximum(false),
	_distribution(GaussianDistribution),
	_normalization(RFNormalization),
	_studentTNu(0.0),
	_distributionTruncation(0.0),
	_spec(spec)
{
	std::cout << "DyscoStMan(name,spec)\n";
	setFromSpec();
}

DyscoStMan::DyscoStMan(const DyscoStMan& source) :
	DataManager(),
	_nRow(0),
	_nBlocksInFile(0),
	_rowsPerBlock(0),
	_antennaCount(0),
	_blockSize(0),
	_headerSize(0),
	_name(source._name),
	_floatBitCount(source._floatBitCount),
	_weightBitCount(source._weightBitCount),
	_fitToMaximum(source._fitToMaximum),
	_distribution(source._distribution),
	_normalization(source._normalization),
	_studentTNu(source._studentTNu),
	_distributionTruncation(source._distributionTruncation),
	_spec(source._spec)
{
	std::cout << "DyscoStMan(source)\n";
	initializeSpec();
}

void DyscoStMan::setFromSpec()
{
	// Here we need to load from _spec
	int i = _spec.description().fieldNumber("floatBitCount");
	if(i >= 0)
	{
		_floatBitCount = _spec.asInt("floatBitCount");
		_weightBitCount = _spec.asInt("weightBitCount");
		_fitToMaximum = _spec.asBool("fitToMaximum");
		std::string str = _spec.asString("distribution");
		if(str == "Uniform")
			_distribution = UniformDistribution;
		else if(str == "Gaussian")
			_distribution = GaussianDistribution;
		else if(str == "StudentT")
			_distribution = StudentsTDistribution;
		else if(str == "TruncatedGaussian")
			_distribution = TruncatedGaussianDistribution;
		else throw std::runtime_error("Unsupported distribution specified");
		str = _spec.asString("normalization");
		if(str == "RF")
			_normalization = RFNormalization;
		else if(str == "AF")
			_normalization = AFNormalization;
		else throw std::runtime_error("Unsupported normalization specified");
		_studentTNu = _spec.asDouble("studentTNu");
		_distributionTruncation = _spec.asDouble("distributionTruncation");
	}
}

void DyscoStMan::initializeSpec()
{
	_spec.define("floatBitCount", _floatBitCount);
	_spec.define("weightBitCount", _weightBitCount);
	_spec.define("fitToMaximum", _fitToMaximum);
	std::string distStr;
	switch(_distribution) {
		case GaussianDistribution: distStr = "Gaussian"; break;
		case UniformDistribution: distStr = "Uniform"; break;
		case StudentsTDistribution: distStr = "StudentsT"; break;
		case TruncatedGaussianDistribution: distStr = "TruncatedGaussian"; break;
	}
	_spec.define("distribution", distStr);
	std::string normStr;
	switch(_normalization) {
		case AFNormalization: normStr = "AF"; break;
		case RFNormalization: normStr = "RF"; break;
	}
	_spec.define("normalization", normStr);
	_spec.define("studentTNu", _studentTNu);
	_spec.define("distributionTruncation", _distributionTruncation);
}

void DyscoStMan::makeEmpty()
{
	for(std::vector<DyscoStManColumn*>::iterator i = _columns.begin(); i!=_columns.end(); ++i)
		delete *i;
	_columns.clear();
}

DyscoStMan::~DyscoStMan()
{
	std::cout << "~DyscoStMan()\n";
	makeEmpty();
}

casacore::Record DyscoStMan::dataManagerSpec() const 
{
	return _spec;
}

void DyscoStMan::registerClass()
{
	DataManager::registerCtor("DyscoStMan", makeObject);
}

casacore::Bool DyscoStMan::flush(casacore::AipsIO&, casacore::Bool doFsync)
{
	return false;
}

void DyscoStMan::create(casacore::uInt nRow)
{
	std::cout << "create(" << fileName() << ")\n";
	_nRow = nRow;
	_fStream.reset(new std::fstream(fileName().c_str(), std::ios_base::in | std::ios_base::out | std::ios_base::trunc));
	if(_fStream->fail())
		throw DyscoStManError("I/O error: could not create new file '" + fileName() + "'");
	_nBlocksInFile = 0;
}

void DyscoStMan::writeHeader()
{
	_fStream->seekp(0, std::ios_base::beg);

	_headerSize = sizeof(Header);	
	for(DyscoStManColumn* col : _columns)
		_headerSize += sizeof(GenericColumnHeader) + col->ExtraHeaderSize();
	
	Header header;
	memset(&header, 0, sizeof(Header));
	header.headerSize = _headerSize;
	header.columnHeaderOffset = sizeof(Header);
	header.columnCount = _columns.size();
	header.versionMajor = VERSION_MAJOR;
	header.versionMinor = VERSION_MINOR;
	header.floatBitCount = _floatBitCount;
	header.weightBitCount = _weightBitCount;
	header.fitToMaximum = _fitToMaximum;
	header.distribution = _distribution;
	header.normalization = _normalization;
	header.studentTNu = _studentTNu;
	header.distributionTruncation = _distributionTruncation;
	header.rowsPerBlock = _rowsPerBlock;
	header.antennaCount = _antennaCount;
	header.blockSize = _blockSize;
	_fStream->write(reinterpret_cast<char*>(&header), sizeof(header));
	for(DyscoStManColumn* col : _columns)
	{
		GenericColumnHeader cHeader;
		cHeader.columnHeaderSize = sizeof(GenericColumnHeader) + col->ExtraHeaderSize();
		_fStream->write(reinterpret_cast<char*>(&cHeader), sizeof(cHeader));
		ao::uvector<unsigned char> extraHeader(col->ExtraHeaderSize());
		col->GetExtraHeader(extraHeader.data());
		_fStream->write(reinterpret_cast<char*>(extraHeader.data()), col->ExtraHeaderSize());
	}
	if(_fStream->fail())
		throw DyscoStManError("I/O error: could not write to file");
}

void DyscoStMan::readHeader()
{
	Header header;
	_fStream->seekg(0, std::ios_base::beg);
	_fStream->read(reinterpret_cast<char*>(&header), sizeof(header));
	_headerSize = header.headerSize;
	size_t curColumnHeaderOffset = header.columnHeaderOffset;
	size_t columnCount = header.columnCount;
	
	_floatBitCount = header.floatBitCount;
	_weightBitCount = header.weightBitCount;
	_fitToMaximum = header.fitToMaximum;
	_distribution = (enum DyscoDistribution) header.distribution;
	_normalization = (enum DyscoNormalization) header.normalization;
	_studentTNu = header.studentTNu;
	_distributionTruncation = header.distributionTruncation;
	_rowsPerBlock = header.rowsPerBlock;
	_antennaCount = header.antennaCount;
	_blockSize = header.blockSize;
	
	if(columnCount != _columns.size())
	{
		std::stringstream s;
		s << "The column count in the DyscoStMan file (" << columnCount << ") does not match with the measurement set (" << _columns.size() << ")";
		throw DyscoStManError(s.str());
	}
	
	for(size_t i = 0; i!=_columns.size(); ++i)
	{
		DyscoStManColumn& col = *_columns[i];
		GenericColumnHeader cHeader;
		_fStream->seekg(curColumnHeaderOffset, std::ios_base::beg);
		_fStream->read(reinterpret_cast<char*>(&cHeader), sizeof(cHeader));
		ao::uvector<unsigned char> extraHeader(col.ExtraHeaderSize());
		_fStream->read(reinterpret_cast<char*>(extraHeader.data()), col.ExtraHeaderSize());
		col.SetFromExtraHeader(extraHeader.data());
		curColumnHeaderOffset += cHeader.columnHeaderSize;
	}
}

void DyscoStMan::initializeRowsPerBlock(size_t rowsPerBlock, size_t antennaCount)
{
	if(areOffsetsInitialized() && (rowsPerBlock != _rowsPerBlock || antennaCount != _antennaCount))
		throw std::runtime_error("initializeRowsPerBlock() called with two different values; something is wrong");
	
	_rowsPerBlock = rowsPerBlock;
	_antennaCount = antennaCount;
	_blockSize = 0;
	for(DyscoStManColumn* col : _columns)
	{
		size_t columnBlockSize = col->CalculateBlockSize(rowsPerBlock, antennaCount);
		col->SetOffsetInBlock(_blockSize);
		_blockSize += columnBlockSize;
		
		//DyscoWeightColumn *wghtCol = dynamic_cast<DyscoWeightColumn*>(*col);
		//if(wghtCol != 0)
		//	wghtCol->SetBitsPerSymbol(_weightBitCount);
		
		col->InitializeAfterNRowsPerBlockIsKnown();
	}
	std::cout << "Total block size: " << _blockSize << '\n';
	
	writeHeader();
}

void DyscoStMan::open(casacore::uInt nRow, casacore::AipsIO&)
{
	std::cout << "open() " << fileName() << '\n';
	_nRow = nRow;
	_fStream.reset(new std::fstream(fileName().c_str()));
	if(_fStream->fail())
		throw DyscoStManError("I/O error: could not open file '" + fileName() + "', which should be an existing file");
	
	readHeader();
	std::cout << "_headerSize=" << _headerSize << "\n";
	
	_fStream->seekg(0, std::ios_base::end);
	if(_fStream->fail())
		throw DyscoStManError("I/O error: error reading file '" + fileName());
	std::streampos size = _fStream->tellg();
	if(size > _headerSize)
		_nBlocksInFile = (size_t(size) - _headerSize) / _blockSize;
	else
		_nBlocksInFile = 0;
}

casacore::DataManagerColumn* DyscoStMan::makeScalarColumn(const casacore::String& name, int dataType, const casacore::String& dataTypeID)
{
	std::ostringstream s;
	s << "Can not create scalar columns with DyscoStMan! (requested datatype: '" << dataTypeID << "' (" << dataType << ")";
	throw DyscoStManError(s.str());
}

casacore::DataManagerColumn* DyscoStMan::makeDirArrColumn(const casacore::String& name, int dataType, const casacore::String& dataTypeID)
{
	std::cout << "make column\n";
	DyscoStManColumn *col = 0;
	
	if(name == "DATA" || name == "CORRECTED_DATA" || name == "MODEL_DATA")
	{
		if(dataType == casacore::TpComplex)
			col = new DyscoDataColumn(this, dataType);
		else
			throw std::runtime_error("Trying to create a Dysco data column with wrong type");
	}
	else if(name == "WEIGHT_SPECTRUM")
	{
		if(dataType == casacore::TpFloat)
			col = new DyscoWeightColumn(this, dataType);
		else
			throw std::runtime_error("Trying to create a Dysco weight column with wrong type");
	}
	else
		throw std::runtime_error("Trying to create a Dysco column, but type is unknown");
	_columns.push_back(col);
	return col;
}

casacore::DataManagerColumn* DyscoStMan::makeIndArrColumn(const casacore::String& name, int dataType, const casacore::String& dataTypeID)
{
	throw DyscoStManError("makeIndArrColumn() called on DyscoStMan. DyscoStMan can only created direct columns!\nUse casacore::ColumnDesc::Direct as option in your column desc constructor");
}

void DyscoStMan::resync(casacore::uInt nRow)
{
	std::cout << "resync()\n";
}

void DyscoStMan::deleteManager()
{
	unlink(fileName().c_str());
}

void DyscoStMan::prepare()
{
	std::cout << "prepare\n";
	
	mutex::scoped_lock lock(_mutex);
	
	if(_floatBitCount == 0 || _weightBitCount == 0)
		throw DyscoStManError("One of the required parameters of the DyscoStMan was not set!\nDyscoStMan was not correctly initialized by your program.");
	
	for(DyscoStManColumn* col : _columns)
	{
		DyscoDataColumn* dataCol = dynamic_cast<DyscoDataColumn*>(col);
		if(dataCol != 0)
			dataCol->SetBitsPerSymbol(_floatBitCount);
		else {
			DyscoWeightColumn* wghtCol = dynamic_cast<DyscoWeightColumn*>(col);
			if(wghtCol != 0)
				wghtCol->SetBitsPerSymbol(_weightBitCount);
		}
		col->Prepare(_fitToMaximum, _distribution, _normalization, _studentTNu, _distributionTruncation);
	}
	
	// In case this is a new measurement set, we do not know the rowsPerBlock yet
	// If this measurement set is opened, we do know it, and we have to call
	// initializeRowsPerBlock() to let the columns know this value.
	if(areOffsetsInitialized())
		initializeRowsPerBlock(_rowsPerBlock, _antennaCount);
}

void DyscoStMan::reopenRW()
{
}

void DyscoStMan::addRow(casacore::uInt nrrow)
{
	_nRow += nrrow;
}

void DyscoStMan::removeRow(casacore::uInt rowNr)
{
	if(rowNr != _nRow-1)
		throw DyscoStManError("Trying to remove a row in the middle of the file: the DyscoStMan does not support this");
	_nRow--;
}

void DyscoStMan::addColumn(casacore::DataManagerColumn* column)
{
	std::cout << "addColumn()\n";
	if(_nBlocksInFile != 0)
		throw DyscoStManError("Can't add columns while data has been committed to table");
	
	//DyscoDataColumn *dataCol = dynamic_cast<DyscoDataColumn*>(column);
	//if(dataCol != 0)
	//	dataCol->SetBitsPerSymbol(_floatBitCount);
	//OffringaWeightColumn *wghtCol = dynamic_cast<OffringaWeightColumn*>(column);
	//if(wghtCol != 0)
	//	wghtCol->SetBitsPerSymbol(_weightBitCount);
	
	prepare();
	writeHeader();
}

void DyscoStMan::removeColumn(casacore::DataManagerColumn* column)
{
	for(std::vector<DyscoStManColumn*>::iterator i=_columns.begin(); i!=_columns.end(); ++i)
	{
		if(*i == column)
		{
			delete *i;
			_columns.erase(i);
			writeHeader();
			return;
		}
	}
	throw DyscoStManError("Trying to remove column that was not part of the storage manager");
}

void DyscoStMan::readCompressedData(size_t blockIndex, const DyscoStManColumn *column, unsigned char* dest, size_t size)
{
	mutex::scoped_lock lock(_mutex);
	size_t fileOffset = getFileOffset(blockIndex);
	
	size_t columnOffset = column->OffsetInBlock();
	_fStream->seekg(fileOffset + columnOffset, std::ios_base::beg);
	_fStream->read(reinterpret_cast<char*>(dest), size);
	if(_fStream->fail())
	{
		// This can be sort of ok ; row exists because other columns have written here,
		// but no data had been written yet for this column
		if(blockIndex+1 != _nBlocksInFile)
			throw DyscoStManError("I/O error: error while reading file '" + fileName() + "'");
		_fStream->clear(); // reset fail bit
	}
}

void DyscoStMan::writeCompressedData(size_t blockIndex, const DyscoStManColumn *column, const unsigned char *data, size_t size)
{
	mutex::scoped_lock lock(_mutex);
	if(_nBlocksInFile <= blockIndex)
	{
		_nBlocksInFile = blockIndex + 1;
	}
	size_t fileOffset = getFileOffset(blockIndex);
	_fStream->seekp(fileOffset + column->OffsetInBlock(), std::ios_base::beg);
	_fStream->write(reinterpret_cast<const char*>(data), size);
	if(_fStream->fail())
		throw DyscoStManError("I/O error: error while writing file '" + fileName() + "'");
}

} // end of namespace