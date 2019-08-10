
#pragma once

class membuf : public streambuf
{
public:
	membuf(char* p, size_t n) : pbuf(p),bufsize(n)
	{
		setg(p, p, p + n);
		setp(p, p + n);
	}

	virtual pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which = std::ios_base::in) override
	{
		if(dir == std::ios_base::cur)
			gbump(off);
		else if(dir == std::ios_base::end)
			setg(pbuf, pbuf+bufsize+off, pbuf+bufsize);
		else if(dir == std::ios_base::beg)
			setg(pbuf, pbuf+off, pbuf+bufsize);

		return gptr() - eback();
	}
    
	virtual pos_type seekpos(streampos pos, std::ios_base::openmode mode) override
	{
		return seekoff(pos - pos_type(off_type(0)), std::ios_base::beg, mode);
	}

	char *pbuf;
	size_t bufsize;
};

