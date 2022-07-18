import torch
from typing import Any, Set, Dict, List, Tuple, OrderedDict

class ModelReportVisualizer:
    r"""
    The ModelReportVisualizer class aims to provide users a way to visualize some of the statistics
    that were generated by the ModelReport API. However, at a higher level, the class aims to provide
    some level of visualization of statistics to PyTorch in order to make it easier to parse data and
    diagnose any potential issues with data or a specific model. With respects to the visualizations,
    the ModelReportVisualizer class currently supports several methods of visualizing data.

    Supported Visualization Methods Include:
    - Table format
    - Plot format (line graph)
    - Histogram format

    For all of the existing visualization methods, there is the option to filter data based on:
    - A module fqn prefix
    - Feature [required for the plot and histogram]

    * :attr:`generated_reports` The reports generated by the ModelReport class in the structure below
        Ensure sure that features that are the same across different report contain the same name
        Ensure that objects representing the same features are the same type / dimension (where applicable)

    Note:
        Currently, the ModelReportVisualizer class supports visualization of data generated by the
        ModelReport class. However, this structure is extensible and should allow the visualization of
        other information as long as the information is structured in the following general format:

        Report Structure
        -- module_fqn [module with attached detectors]
            |
            -- feature keys [not every detector extracts same information]
                                    [same collected info has same keys, unless can be specific to detector]


    The goal behind the class is that the generated visualizations can be used in conjunction with the generated
    report for people to get a better understand of issues and what the fix might be. It is also just to provide
    a good visualization platform, since it might be hard to parse through the ModelReport returned dictionary as
    that grows in size.

    General Use Flow Expected
    1.) Initialize ModelReport object with reports of interest by passing in initialized detector objects
    2.) Prepare your model with prepare_fx
    3.) Call model_report.prepare_detailed_calibration on your model to add relavent observers
    4.) Callibrate your model with data
    5.) Call model_report.generate_report on your model to generate report and optionally remove added observers
    6.) Use output of model_report.generate_report to initialize ModelReportVisualizer instance
    7.) Use instance to view different views of data as desired, applying filters as needed

    """

    def __init__(self, generated_reports: OrderedDict[str, Any]):
        r"""
        Initializes the ModelReportVisualizer instance with the necessary reports.

        Args:
            generated_reports (Dict[str, Any]): The reports generated by the ModelReport class
                can also be a dictionary generated in another manner, as long as format is same
        """
        self.generated_reports = generated_reports

    def get_all_unique_module_fqns(self) -> Set[str]:
        r"""
        The purpose of this method is to provide a user the set of all module_fqns so that if
        they wish to use some of the filtering capabilities of the ModelReportVisualizer class,
        they don't need to manually parse the generated_reports dictionary to get this information.

        Returns all the unique module fqns present in the reports the ModelReportVisualizer
        instance was initialized with.
        """
        # returns the keys of the ordered dict
        return set(self.generated_reports.keys())

    def get_all_unique_feature_names(self, plottable: bool) -> Set[str]:
        r"""
        The purpose of this method is to provide a user the set of all feature names so that if
        they wish to use the filtering capabilities of the generate_table_view(), or use either of
        the generate_plot_view() or generate_histogram_view(), they don't need to manually parse
        the generated_reports dictionary to get this information.

        Args:
            plottable (bool): True if the user is only looking for plottable features, False otherwise
                plottable features are those that are tensor values

        Returns all the unique module fqns present in the reports the ModelReportVisualizer
        instance was initialized with.
        """
        unique_feature_names = set()
        for module_fqn in self.generated_reports:
            # get dict of the features
            feature_dict: Dict[str, Any] = self.generated_reports[module_fqn]

            # loop through features
            for feature_name in feature_dict:
                # if we need plottable, ensure type of val is tensor
                if plottable:
                    if type(feature_dict[feature_name]) == torch.Tensor:
                        unique_feature_names.add(feature_name)
                else:
                    # any and all features
                    unique_feature_names.add(feature_name)

        # return our compiled set of unique feature names
        return unique_feature_names

    def generate_table_view(self, feature: str = "", module_fqn_prefix_filter: str = "") -> Tuple[List[List[Any]], str]:
        r"""
        Takes in optional filter values and generates a table with the desired information.

        The generated table is presented in both a list-of-lists format for further manipulation and filtering
        as well as a formatted string that is ready to print

        Table columns:

         idx  layer_fqn   type  shape  feature_1   feature_2   feature_3   .... feature_n
        ----  ---------   ----  -----  ---------   ---------   ---------        ---------

        Args:
            feature (str, optional): The specific feature we wish to generate the table for
                Default = "", results in all the features being printed
            module_fqn_prefix_filter (str, optional): Only includes modules with this string prefix
                Default = "", results in all the modules in the reports to be visible in the table

        Returns a tuple with two objects:
            (List[List[Any]]) A list of lists containing the table information row by row
                The 0th index row will contain the headers of the columns
                The rest of the rows will contain data
            (str) The formatted string that contains the table information to be printed
        Expected Use:
            >>> info, tabluated_str = model_report_visualizer.generate_table_view(*filters)
            >>> print(tabulated_str) # outputs neatly formatted table
        """
        pass

    def generate_plot_view(self, feature: str, module_fqn_prefix_filter: str = "") -> List[List[Any]]:
        r"""
        Takes in a feature and optional module_filter and generates a line plot of the desired data.

        Note:
            Only features in the report that have tensor value data are plottable by this class

        Args:
            feature (str): The specific feature we wish to generate the plot for
            module_fqn_prefix_filter (str, optional): Only includes modules with this string prefix
                Default = "", results in all the modules in the reports to be visible in the plot

        Returns a tuple with two objects:
            (List[List[Any]]) A list of lists containing the plot information row by row
                The 0th index row will contain the headers of the columns
                The rest of the rows will contain data
        Expected Use:
            >>> # the code below both returns the info and diplays the plot
            >>> info = model_report_visualizer.generate_plot_view(*filters)
        """
        pass

    def generate_histogram_view(self, feature: str, module_fqn_prefix_filter: str = "") -> List[List[Any]]:
        r"""
        Takes in a feature and optional module_filter and generates a histogram of the desired data.

        Note:
            Only features in the report that have tensor value data can be viewed as a histogram

        Args:
            feature (str): The specific feature we wish to generate the plot for
            module_fqn_prefix_filter (str, optional): Only includes modules with this string prefix
                Default = "", results in all the modules in the reports to be visible in the histogram

        Returns a tuple with two objects:
            (List[List[Any]]) A list of lists containing the histogram information row by row
                The 0th index row will contain the headers of the columns
                The rest of the rows will contain data
        Expected Use:
            >>> # the code below both returns the info and displays the histogram
            >>> info = model_report_visualizer.generate_histogram_view(*filters)
        """
        pass
