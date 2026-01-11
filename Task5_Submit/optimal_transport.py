"""
Optimal Transport Module for Pixel Sculptor
Implements efficient OT algorithms for image transformation
"""

import numpy as np
from scipy.optimize import linear_sum_assignment
from scipy.spatial.distance import cdist
import logging

logger = logging.getLogger(__name__)


def sinkhorn_transport(source_pixels, target_pixels, reg=0.1, max_iter=100, tol=1e-6):
    """
    Sinkhorn algorithm for entropy-regularized optimal transport.
    Much faster than Hungarian for large problems.
    
    Args:
        source_pixels: (N, 3) array of source RGB values
        target_pixels: (M, 3) array of target RGB values  
        reg: Regularization parameter (higher = faster but less optimal)
        max_iter: Maximum iterations
        tol: Convergence tolerance
    
    Returns:
        Transport plan matrix P
    """
    n, m = len(source_pixels), len(target_pixels)
    
    # Cost matrix (Euclidean distance in color space)
    C = cdist(source_pixels, target_pixels, metric='euclidean')
    
    # Normalize cost
    C = C / C.max()
    
    # Kernel matrix
    K = np.exp(-C / reg)
    
    # Initialize
    u = np.ones(n) / n
    v = np.ones(m) / m
    a = np.ones(n) / n  # Source marginal
    b = np.ones(m) / m  # Target marginal
    
    # Sinkhorn iterations
    for i in range(max_iter):
        u_prev = u.copy()
        
        u = a / (K @ v + 1e-10)
        v = b / (K.T @ u + 1e-10)
        
        # Check convergence
        if np.max(np.abs(u - u_prev)) < tol:
            logger.debug(f"Sinkhorn converged at iteration {i}")
            break
    
    # Transport plan
    P = np.diag(u) @ K @ np.diag(v)
    
    return P


def hungarian_transport(source_pixels, target_pixels, sample_size=2000):
    """
    Hungarian algorithm for exact optimal transport.
    Uses sampling for large images.
    
    Args:
        source_pixels: (N, 3) array of source RGB values
        target_pixels: (M, 3) array of target RGB values
        sample_size: Max pixels to use (for memory efficiency)
    
    Returns:
        tuple: (row_indices, col_indices) for optimal assignment
    """
    n = min(len(source_pixels), len(target_pixels), sample_size)
    
    # Sample if needed
    if len(source_pixels) > n:
        src_idx = np.linspace(0, len(source_pixels)-1, n, dtype=int)
        src = source_pixels[src_idx]
    else:
        src_idx = np.arange(len(source_pixels))
        src = source_pixels
    
    if len(target_pixels) > n:
        tgt_idx = np.linspace(0, len(target_pixels)-1, n, dtype=int)
        tgt = target_pixels[tgt_idx]
    else:
        tgt_idx = np.arange(len(target_pixels))
        tgt = target_pixels
    
    # Cost matrix
    logger.info(f"Computing cost matrix for {n}x{n} pixels...")
    C = cdist(src, tgt, metric='euclidean')
    
    # Hungarian algorithm
    logger.info("Running Hungarian algorithm...")
    row_ind, col_ind = linear_sum_assignment(C)
    
    # Map back to original indices
    return src_idx[row_ind], tgt_idx[col_ind]


def blockwise_transport(source_img, target_img, block_size=16):
    """
    Block-wise optimal transport for memory efficiency.
    Processes image in blocks instead of all pixels at once.
    
    Args:
        source_img: (H, W, 3) source image array
        target_img: (H, W, 3) target image array
        block_size: Size of blocks to process
    
    Returns:
        Transformed image array
    """
    h, w = source_img.shape[:2]
    result = np.zeros_like(target_img)
    
    n_blocks_h = (h + block_size - 1) // block_size
    n_blocks_w = (w + block_size - 1) // block_size
    total_blocks = n_blocks_h * n_blocks_w
    
    logger.info(f"Processing {total_blocks} blocks of size {block_size}x{block_size}")
    
    for i in range(n_blocks_h):
        for j in range(n_blocks_w):
            y1, y2 = i * block_size, min((i + 1) * block_size, h)
            x1, x2 = j * block_size, min((j + 1) * block_size, w)
            
            src_block = source_img[y1:y2, x1:x2].reshape(-1, 3)
            tgt_block = target_img[y1:y2, x1:x2].reshape(-1, 3)
            
            # Compute transport for this block
            P = sinkhorn_transport(src_block, tgt_block, reg=0.05)
            
            # Apply transport: weighted sum based on transport plan
            block_h, block_w = y2 - y1, x2 - x1
            transported = np.zeros((block_h * block_w, 3))
            
            for k in range(len(tgt_block)):
                # Weighted average of source pixels
                weights = P[:, k]
                if weights.sum() > 0:
                    weights = weights / weights.sum()
                    transported[k] = (weights[:, np.newaxis] * src_block).sum(axis=0)
                else:
                    transported[k] = tgt_block[k]
            
            result[y1:y2, x1:x2] = transported.reshape(block_h, block_w, 3)
    
    return result


def color_histogram_transport(source_img, target_img, n_bins=64):
    """
    Color histogram matching using OT.
    Fast approximate transport based on color distributions.
    
    Args:
        source_img: Source image array
        target_img: Target image array
        n_bins: Number of bins for histogram
    
    Returns:
        Transformed image
    """
    result = source_img.copy().astype(float)
    
    # Process each channel separately
    for c in range(3):
        src_channel = source_img[:, :, c].flatten()
        tgt_channel = target_img[:, :, c].flatten()
        
        # Compute histograms
        src_hist, src_bins = np.histogram(src_channel, bins=n_bins, range=(0, 255))
        tgt_hist, tgt_bins = np.histogram(tgt_channel, bins=n_bins, range=(0, 255))
        
        # Compute CDFs
        src_cdf = np.cumsum(src_hist).astype(float) / src_hist.sum()
        tgt_cdf = np.cumsum(tgt_hist).astype(float) / tgt_hist.sum()
        
        # Create mapping
        mapping = np.zeros(n_bins)
        for i in range(n_bins):
            # Find target bin with closest CDF value
            j = np.argmin(np.abs(tgt_cdf - src_cdf[i]))
            mapping[i] = (tgt_bins[j] + tgt_bins[j + 1]) / 2
        
        # Apply mapping
        src_bins_idx = np.digitize(src_channel, src_bins[:-1]) - 1
        src_bins_idx = np.clip(src_bins_idx, 0, n_bins - 1)
        result[:, :, c] = mapping[src_bins_idx].reshape(source_img.shape[:2])
    
    return np.clip(result, 0, 255).astype(np.uint8)


def apply_transport_plan(source_img, target_img, method='blockwise'):
    """
    Main function to apply optimal transport transformation.
    
    Args:
        source_img: Source PIL Image or numpy array
        target_img: Target PIL Image or numpy array
        method: 'blockwise', 'histogram', or 'hungarian'
    
    Returns:
        Transformed numpy array
    """
    # Convert to numpy if needed
    if hasattr(source_img, 'convert'):
        source_img = np.array(source_img.convert('RGB'))
    if hasattr(target_img, 'convert'):
        target_img = np.array(target_img.convert('RGB'))
    
    # Resize if dimensions don't match
    if source_img.shape != target_img.shape:
        from PIL import Image
        src_pil = Image.fromarray(source_img)
        src_pil = src_pil.resize((target_img.shape[1], target_img.shape[0]))
        source_img = np.array(src_pil)
        logger.info(f"Resized source to {source_img.shape}")
    
    logger.info(f"Applying {method} transport on {source_img.shape} image")
    
    if method == 'blockwise':
        return blockwise_transport(source_img, target_img, block_size=8)
    elif method == 'histogram':
        return color_histogram_transport(source_img, target_img)
    elif method == 'hungarian':
        # Full Hungarian is expensive, use for small images only
        src_flat = source_img.reshape(-1, 3)
        tgt_flat = target_img.reshape(-1, 3)
        
        src_idx, tgt_idx = hungarian_transport(src_flat, tgt_flat)
        
        result = np.zeros_like(target_img)
        h, w = target_img.shape[:2]
        
        for si, ti in zip(src_idx, tgt_idx):
            ty, tx = ti // w, ti % w
            result[ty, tx] = src_flat[si]
        
        return result
    else:
        raise ValueError(f"Unknown method: {method}")
