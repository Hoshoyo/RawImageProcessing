
int get_index(int x, int y, int width, int height, int channel) {
	if (x < 0) x += 2;
	if (x >= width) x -= 2;
	if (y < 0) y += 2;
	if (y >= height) y -= 2;
	return (y * 4 * width + x * 4 + channel);
}

int load_raw(RawImage* loaded_image) {
	LibRaw& img = *loaded_image->raw;

	int width = img.imgdata.sizes.iwidth;
	int height = img.imgdata.sizes.iheight;
	short* ptr = (short*)img.imgdata.image;

	void* data = calloc(4, width * height * sizeof(r32));

	ushort maxvalue = 0;

	for (int i = 0; i < width * height; ++i) {
		s32 x = i % width;
		s32 y = i / width;

		r32 sr = (r32)ptr[get_index(x, y, width, height, 0)];
		r32 sg = (r32)ptr[get_index(x, y, width, height, 1)];
		r32 sb = (r32)ptr[get_index(x, y, width, height, 2)];
		r32 sa = (r32)ptr[get_index(x, y, width, height, 3)];

		if (sr > maxvalue) maxvalue = sr;
		if (sg > maxvalue) maxvalue = sg;
		if (sb > maxvalue) maxvalue = sb;
		if (sa > maxvalue) maxvalue = sa;
	}
	for (int i = 0; i < width * height; ++i) {
		s32 x = i % width;
		s32 y = i / width;

		r32 sr = (r32)ptr[get_index(x, y, width, height, 0)] / maxvalue;
		r32 sg = (r32)ptr[get_index(x, y, width, height, 1)] / maxvalue;
		r32 sb = (r32)ptr[get_index(x, y, width, height, 2)] / maxvalue;
		r32 sa = (r32)ptr[get_index(x, y, width, height, 3)] / maxvalue;

		((r32*)data)[i * 4 + 0] = sr;
		((r32*)data)[i * 4 + 1] = sg + sa;
		((r32*)data)[i * 4 + 2] = sb;
		((r32*)data)[i * 4 + 3] = 1.0f;
	}
	loaded_image->demosaic_data = (r32*)data;
	u32 id = create_texture(width, height, data);
	return id;
}

int demosaic(RawImage* loaded_image) {
	LibRaw& img = *loaded_image->raw;

	int width = img.imgdata.sizes.iwidth;
	int height = img.imgdata.sizes.iheight;

	void* data = calloc(4, width * height * sizeof(r32));
	short* ptr = (short*)img.imgdata.image;

	for (int i = 0; i < width * height; ++i) {
		s32 x = i % width;
		s32 y = i / width;

		ushort sr = ptr[get_index(x, y, width, height, 0)];
		ushort sg = ptr[get_index(x, y, width, height, 1)];
		ushort sb = ptr[get_index(x, y, width, height, 2)];
		ushort sa = ptr[get_index(x, y, width, height, 3)];

		r32 r = (r32)sr;
		r32 g = (r32)sg;
		r32 b = (r32)sb;
		r32 a = (r32)sa;

		bool pair_line = (y % 2 == 0);
		bool pair_column = (x % 2 == 0);

		if (!pair_column && !pair_line) {
			// 4 cantos
			r32 r_tl = ptr[get_index(x - 1, y - 1, width, height, 0)];
			r32 r_tr = ptr[get_index(x + 1, y - 1, width, height, 0)];
			r32 r_bl = ptr[get_index(x - 1, y + 1, width, height, 0)];
			r32 r_br = ptr[get_index(x + 1, y + 1, width, height, 0)];
			r = (((r_tl + r_tr) / 2.0f) + ((r_bl + r_br) / 2.0f)) / 2.0f;

			r32 g_l = (ptr[get_index(x - 1, y, width, height, 1)] + ptr[get_index(x - 1, y, width, height, 3)]);
			r32 g_r = (ptr[get_index(x + 1, y, width, height, 1)] + ptr[get_index(x + 1, y, width, height, 3)]);
			r32 g_t = (ptr[get_index(x, y - 1, width, height, 1)] + ptr[get_index(x, y - 1, width, height, 3)]);
			r32 g_b = (ptr[get_index(x, y + 1, width, height, 1)] + ptr[get_index(x, y + 1, width, height, 3)]);
			g = (((g_l + g_r) / 2.0f) + ((g_t + g_b) / 2.0f)) / 2.0f;
		}
		if (!pair_column && pair_line) {
			r32 r_l = ptr[get_index(x - 1, y, width, height, 0)];
			r32 r_r = ptr[get_index(x + 1, y, width, height, 0)];
			r = (r_l + r_r) / 2.0f;

			r32 b_t = ptr[get_index(x, y - 1, width, height, 2)];
			r32 b_b = ptr[get_index(x, y + 1, width, height, 2)];
			b = (b_t + b_b) / 2.0f;
		}
		if (pair_column && !pair_line) {
			r32 r_t = ptr[get_index(x, y - 1, width, height, 0)];
			r32 r_b = ptr[get_index(x, y + 1, width, height, 0)];
			r = (r_t + r_b) / 2.0f;

			r32 b_l = ptr[get_index(x - 1, y, width, height, 2)];
			r32 b_r = ptr[get_index(x + 1, y, width, height, 2)];
			b = (b_l + b_r) / 2.0f;
		}
		if (pair_column && pair_line) {
			r32 g_l = (ptr[get_index(x - 1, y, width, height, 1)] + ptr[get_index(x - 1, y, width, height, 3)]);
			r32 g_r = (ptr[get_index(x + 1, y, width, height, 1)] + ptr[get_index(x + 1, y, width, height, 3)]);
			r32 g_t = (ptr[get_index(x, y - 1, width, height, 1)] + ptr[get_index(x, y - 1, width, height, 3)]);
			r32 g_b = (ptr[get_index(x, y + 1, width, height, 1)] + ptr[get_index(x, y + 1, width, height, 3)]);
			g = (((g_l + g_r) / 2.0f) + ((g_t + g_b) / 2.0f)) / 2.0f;

			r32 b_tl = ptr[get_index(x - 1, y - 1, width, height, 2)];
			r32 b_tr = ptr[get_index(x + 1, y - 1, width, height, 2)];
			r32 b_bl = ptr[get_index(x - 1, y + 1, width, height, 2)];
			r32 b_br = ptr[get_index(x + 1, y + 1, width, height, 2)];
			b = (((b_tl + b_tr) / 2.0f) + ((b_bl + b_br) / 2.0f)) / 2.0f;
		}

		((r32*)data)[i * 4 + 0] = r;
		((r32*)data)[i * 4 + 1] = g + a;
		((r32*)data)[i * 4 + 2] = b;
		((r32*)data)[i * 4 + 3] = 1.0f;
	}
	
	// Hardcoded pixel value in the paper
	r32 wr = 1.0f / ((r32*)data)[get_index(1760, 2431, width, height, 0)];
	r32 wg = 1.0f / ((r32*)data)[get_index(1760, 2431, width, height, 1)];
	r32 wb = 1.0f / ((r32*)data)[get_index(1760, 2431, width, height, 2)];

	r32 wb_max = 0.0f;

	for (int i = 0; i < width * height; ++i) {
		s32 x = i % width;
		s32 y = i / width;

		r32 r = ((r32*)data)[get_index(x, y, width, height, 0)];
		r32 g = ((r32*)data)[get_index(x, y, width, height, 1)];
		r32 b = ((r32*)data)[get_index(x, y, width, height, 2)];

		r *= wr;
		g *= wg;
		b *= wb;

		r = powf(r, 1.0f / 2.2f);
		g = powf(g, 1.0f / 2.2f);
		b = powf(b, 1.0f / 2.2f);

		if (r > wb_max) wb_max = r;
		if (g > wb_max) wb_max = g;
		if (b > wb_max) wb_max = b;

		((r32*)data)[get_index(x, y, width, height, 0)] = r;
		((r32*)data)[get_index(x, y, width, height, 1)] = g;
		((r32*)data)[get_index(x, y, width, height, 2)] = b;
	}

	for (int i = 0; i < width * height; ++i) {
		s32 x = i % width;
		s32 y = i / width;

		r32 r = ((r32*)data)[get_index(x, y, width, height, 0)];
		r32 g = ((r32*)data)[get_index(x, y, width, height, 1)];
		r32 b = ((r32*)data)[get_index(x, y, width, height, 2)];

		((r32*)data)[get_index(x, y, width, height, 0)] = r / wb_max;
		((r32*)data)[get_index(x, y, width, height, 1)] = g / wb_max;
		((r32*)data)[get_index(x, y, width, height, 2)] = b / wb_max;

		assert(r / wb_max <= 1.0f);
		assert(g / wb_max <= 1.0f);
		assert(b / wb_max <= 1.0f);
	}
	loaded_image->demosaic_data = (r32*)data;
	u32 id = create_texture(width, height, data);
	return id;
}