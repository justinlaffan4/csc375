#pragma once

uint64_t  vmem_page_size();
void     *vmem_reserve  (uint64_t size);
bool      vmem_commit   (void *base, uint64_t size);
void      vmem_decommit (void *base, uint64_t size);
void      vmem_release  (void *base);

