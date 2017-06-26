import itertools
import bisect


class Dataset(object):
    """An abstract class representing a Dataset.

    All other datasets should subclass it. All subclasses should override
    ``__len__``, that provides the size of the dataset, and ``__getitem__``,
    supporting integer indexing in range from 0 to len(self) exclusive.
    """

    def __getitem__(self, index):
        raise NotImplementedError

    def __len__(self):
        raise NotImplementedError


class TensorDataset(Dataset):
    """Dataset wrapping data and target tensors.

    Each sample will be retrieved by indexing both tensors along the first
    dimension.

    Arguments:
        data_tensor (Tensor): contains sample data.
        target_tensor (Tensor): contains sample targets (labels).
    """

    def __init__(self, data_tensor, target_tensor):
        assert data_tensor.size(0) == target_tensor.size(0)
        self.data_tensor = data_tensor
        self.target_tensor = target_tensor

    def __getitem__(self, index):
        return self.data_tensor[index], self.target_tensor[index]

    def __len__(self):
        return self.data_tensor.size(0)


class ConcatDataset(Dataset):
    """
    Dataset to concatenate multiple datasets.
    Purpose: useful to assemble different existing datasets, possibly
    large-scale datasets as the concatenation operation is done in an
    on-the-fly manner.
    Args:
        datasets (iterable): List of datasets to be concatenated
    """

    def __init__(self, datasets):
        super(ConcatDataset, self).__init__()
        self.datasets = list(datasets)
        assert len(datasets) > 0, 'datasets should not be an empty iterable'
        self.cum_sizes = list(itertools.accumulate(
                              [len(s) for s in self.datasets]))

    def __len__(self):
        return self.cum_sizes[-1]

    def __getitem__(self, idx):
        dataset_idx = bisect.bisect_right(self.cum_sizes, idx)
        if dataset_idx == 0:
            sample_idx = idx
        else:
            sample_idx = idx - self.cum_sizes[dataset_idx - 1]
        return self.datasets[dataset_idx][sample_idx]
