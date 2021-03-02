#!/usr/bin/env python3

import argparse
import bz2
import json
import subprocess
from collections import defaultdict
from datetime import datetime
from typing import Any, Dict, List, Optional, Set, Tuple, Union, cast

import boto3  # type: ignore[import]
import botocore  # type: ignore[import]
from typing_extensions import Literal, TypedDict


def get_git_commit_history(
    *,
    path: str,
    ref: str
) -> List[Tuple[str, datetime]]:
    rc = subprocess.check_output(
        ['git', '-C', path, 'log', '--pretty=format:%H %ct', ref],
    ).decode("latin-1")
    return [
        (x[0], datetime.fromtimestamp(int(x[1])))
        for x in [line.split(" ") for line in rc.split("\n")]
    ]


def get_object_summaries(*, bucket: Any, sha: str) -> Dict[str, List[Any]]:
    summaries = list(bucket.objects.filter(Prefix=f'test_time/{sha}/'))
    by_job = defaultdict(list)
    for summary in summaries:
        job = summary.key.split('/')[2]
        by_job[job].append(summary)
    return dict(by_job)


# TODO: consolidate these typedefs with the identical ones in
# torch/testing/_internal/print_test_stats.py

Commit = str  # 40-digit SHA-1 hex string
Status = Optional[Literal['errored', 'failed', 'skipped']]


class CaseMeta(TypedDict):
    seconds: float


class Version1Case(CaseMeta):
    name: str
    errored: bool
    failed: bool
    skipped: bool


class Version1Suite(TypedDict):
    total_seconds: float
    cases: List[Version1Case]


class ReportMetaMeta(TypedDict):
    build_pr: str
    build_tag: str
    build_sha1: Commit
    build_branch: str
    build_job: str
    build_workflow_id: str


class ReportMeta(ReportMetaMeta):
    total_seconds: float


class Version1Report(ReportMeta):
    suites: Dict[str, Version1Suite]


class Version2Case(CaseMeta):
    status: Status


class Version2Suite(TypedDict):
    total_seconds: float
    cases: Dict[str, Version2Case]


class Version2File(TypedDict):
    total_seconds: float
    suites: Dict[str, Version2Suite]


class VersionedReport(ReportMeta):
    format_version: int


# report: Version2Report implies report['format_version'] == 2
class Version2Report(VersionedReport):
    files: Dict[str, Version2File]


Report = Union[Version1Report, VersionedReport]


def get_jsons(
    jobs: Optional[List[str]],
    summaries: Dict[str, Any],
) -> Dict[str, Report]:
    if jobs is None:
        keys = sorted(summaries.keys())
    else:
        keys = [job for job in jobs if job in summaries]
    return {
        job: json.loads(bz2.decompress(summaries[job].get()['Body'].read()))
        for job in keys
    }


# TODO: consolidate this with the case_status function from
# torch/testing/_internal/print_test_stats.py
def case_status(case: Version1Case) -> Status:
    for k in {'errored', 'failed', 'skipped'}:
        if case[k]:  # type: ignore[misc]
            return cast(Status, k)
    return None


# TODO: consolidate this with the newify_case function from
# torch/testing/_internal/print_test_stats.py
def newify_case(case: Version1Case) -> Version2Case:
    return {
        'seconds': case['seconds'],
        'status': case_status(case),
    }


# TODO: consolidate this with the simplify function from
# torch/testing/_internal/print_test_stats.py
def get_cases(
    *,
    data: Report,
    filename: Optional[str],
    suite_name: Optional[str],
    test_name: str,
) -> List[Version2Case]:
    cases: List[Version2Case] = []
    if 'format_version' not in data:  # version 1 implicitly
        v1report = cast(Version1Report, data)
        suites = v1report['suites']
        for sname, v1suite in suites.items():
            if sname == suite_name or not suite_name:
                for v1case in v1suite['cases']:
                    if v1case['name'] == test_name:
                        cases.append(newify_case(v1case))
    else:
        v_report = cast(VersionedReport, data)
        version = v_report['format_version']
        if version == 2:
            v2report = cast(Version2Report, v_report)
            for fname, v2file in v2report['files'].items():
                if fname == filename or not filename:
                    for sname, v2suite in v2file['suites'].items():
                        if sname == suite_name or not suite_name:
                            v2case = v2suite['cases'].get(test_name)
                            if v2case:
                                cases.append(v2case)
        else:
            raise RuntimeError(f'Unknown format version: {version}')
    return cases


def make_column(
    *,
    data: Optional[Report],
    filename: Optional[str],
    suite_name: Optional[str],
    test_name: str,
    digits: int,
) -> Tuple[str, int]:
    decimals = 3
    num_length = digits + 1 + decimals
    if data:
        cases = get_cases(
            data=data,
            filename=filename,
            suite_name=suite_name,
            test_name=test_name
        )
        if cases:
            case = cases[0]
            status = case['status']
            omitted = len(cases) - 1
            if status:
                return f'{status.rjust(num_length)} ', omitted
            else:
                return f'{case["seconds"]:{num_length}.{decimals}f}s', omitted
        else:
            return f'{"absent".rjust(num_length)} ', 0
    else:
        return ' ' * (num_length + 1), 0


def make_columns(
    *,
    jobs: List[str],
    jsons: Dict[str, Report],
    omitted: Dict[str, int],
    filename: Optional[str],
    suite_name: Optional[str],
    test_name: str,
    digits: int,
) -> str:
    columns = []
    total_omitted = 0
    total_suites = 0
    for job in jobs:
        data = jsons.get(job)
        column, omitted_suites = make_column(
            data=data,
            filename=filename,
            suite_name=suite_name,
            test_name=test_name,
            digits=digits,
        )
        columns.append(column)
        total_suites += omitted_suites
        if job in omitted:
            total_omitted += omitted[job]
    if total_omitted > 0:
        columns.append(f'({total_omitted} S3 reports omitted)')
    if total_suites > 0:
        columns.append(f'({total_suites}) matching suites omitted)')
    return ' '.join(columns)


def make_lines(
    *,
    jobs: Set[str],
    jsons: Dict[str, Report],
    omitted: Dict[str, int],
    filename: Optional[str],
    suite_name: Optional[str],
    test_name: str,
) -> List[str]:
    lines = []
    for job, data in jsons.items():
        cases = get_cases(
            data=data,
            filename=filename,
            suite_name=suite_name,
            test_name=test_name,
        )
        if cases:
            case = cases[0]
            status = case['status']
            line = f'{job} {case["seconds"]}s{f" {status}" if status else ""}'
            if job in omitted and omitted[job] > 0:
                line += f' ({omitted[job]} S3 reports omitted)'
            if len(cases) > 1:
                line += f' ({len(cases) - 1} matching suites omitted)'
            lines.append(line)
        elif job in jobs:
            lines.append(f'{job} (test not found)')
    if lines:
        return lines
    else:
        return ['(no reports in S3)']


def display_history(
    *,
    bucket: Any,
    commits: List[Tuple[str, datetime]],
    jobs: Optional[List[str]],
    filename: Optional[str],
    suite_name: Optional[str],
    test_name: str,
    delta: int,
    sha_length: int,
    mode: str,
    digits: int,
) -> None:
    prev_time = datetime.now()
    for sha, time in commits:
        if (prev_time - time).total_seconds() < delta * 3600:
            continue
        prev_time = time
        summaries = get_object_summaries(bucket=bucket, sha=sha)
        # we assume that get_object_summaries doesn't return empty lists
        jsons = get_jsons(
            jobs=jobs,
            summaries={job: l[0] for job, l in summaries.items()},
        )
        omitted = {
            job: len(l) - 1
            for job, l in summaries.items()
            if len(l) > 1
        }
        if mode == 'columns':
            assert jobs is not None
            lines = [make_columns(
                jobs=jobs,
                jsons=jsons,
                omitted=omitted,
                filename=filename,
                suite_name=suite_name,
                test_name=test_name,
                digits=digits,
            )]
        else:
            assert mode == 'multiline'
            lines = make_lines(
                jobs=set(jobs or []),
                jsons=jsons,
                omitted=omitted,
                filename=filename,
                suite_name=suite_name,
                test_name=test_name,
            )
        for line in lines:
            print(f"{time} {sha[:sha_length]} {line}".rstrip())


class HelpFormatter(
    argparse.ArgumentDefaultsHelpFormatter,
    argparse.RawDescriptionHelpFormatter,
):
    pass


def main() -> None:
    parser = argparse.ArgumentParser(
        __file__,
        description='''
Display the history of a test.

Each line of (non-error) output starts with the timestamp and SHA1 hash
of the commit it refers to, in this format:

    YYYY-MM-DD hh:mm:ss 0123456789abcdef0123456789abcdef01234567

In multiline mode, each line next includes the name of a CircleCI job,
followed by the time of the specified test in that job at that commit.
Example:

    $ tools/test_history.py multiline --ref=594a66 --sha-length=8 \\
      test_set_dir pytorch_linux_xenial_py3_6_gcc{5_4,7}_test
    2021-02-10 03:13:34 594a66d7 pytorch_linux_xenial_py3_6_gcc5_4_test 0.36s
    2021-02-10 03:13:34 594a66d7 pytorch_linux_xenial_py3_6_gcc7_test 0.573s errored
    2021-02-10 02:13:25 9c0caf03 pytorch_linux_xenial_py3_6_gcc5_4_test 0.819s
    2021-02-10 02:13:25 9c0caf03 pytorch_linux_xenial_py3_6_gcc7_test 0.449s
    2021-02-10 02:09:14 602434bc pytorch_linux_xenial_py3_6_gcc5_4_test 0.361s
    2021-02-10 02:09:14 602434bc pytorch_linux_xenial_py3_6_gcc7_test 0.454s
    2021-02-10 02:09:10 2e35fe95 (no reports in S3)
    2021-02-10 02:09:07 ff73be7e (no reports in S3)
    2021-02-10 02:05:39 74082f0d (no reports in S3)
    2021-02-09 23:42:29 0620c96f pytorch_linux_xenial_py3_6_gcc5_4_test 0.414s (1 S3 reports omitted)
    2021-02-09 23:42:29 0620c96f pytorch_linux_xenial_py3_6_gcc7_test 0.377s (1 S3 reports omitted)

Another multiline example, this time with the --all flag:

    $ tools/test_history.py multiline --all --ref=321b9 --delta=12 --sha-length=8 \\
      test_qr_square_many_batched_complex_cuda
    2021-01-07 02:04:56 321b9883 pytorch_linux_xenial_cuda10_2_cudnn7_py3_gcc7_test2 424.284s
    2021-01-07 02:04:56 321b9883 pytorch_linux_xenial_cuda10_2_cudnn7_py3_slow_test 0.006s skipped
    2021-01-07 02:04:56 321b9883 pytorch_linux_xenial_cuda11_1_cudnn8_py3_gcc7_test 402.572s
    2021-01-07 02:04:56 321b9883 pytorch_linux_xenial_cuda9_2_cudnn7_py3_gcc7_test 287.164s
    2021-01-06 12:58:28 fcb69d2e pytorch_linux_xenial_cuda10_2_cudnn7_py3_gcc7_test2 436.732s
    2021-01-06 12:58:28 fcb69d2e pytorch_linux_xenial_cuda10_2_cudnn7_py3_slow_test 0.006s skipped
    2021-01-06 12:58:28 fcb69d2e pytorch_linux_xenial_cuda11_1_cudnn8_py3_gcc7_test 407.616s
    2021-01-06 12:58:28 fcb69d2e pytorch_linux_xenial_cuda9_2_cudnn7_py3_gcc7_test 287.044s

In columns mode, the name of the job isn't printed, but the order of the
columns is guaranteed to match the order of the jobs passed on the
command line. Example:

    $ tools/test_history.py columns --ref=3cf783 --sha-length=8 \\
      test_set_dir pytorch_linux_xenial_py3_6_gcc{5_4,7}_test
    2021-02-10 04:18:50 3cf78395    0.644s    0.312s
    2021-02-10 03:13:34 594a66d7    0.360s  errored
    2021-02-10 02:13:25 9c0caf03    0.819s    0.449s
    2021-02-10 02:09:14 602434bc    0.361s    0.454s
    2021-02-10 02:09:10 2e35fe95
    2021-02-10 02:09:07 ff73be7e
    2021-02-10 02:05:39 74082f0d
    2021-02-09 23:42:29 0620c96f    0.414s    0.377s (2 S3 reports omitted)
    2021-02-09 23:27:53 33afb5f1    0.381s    0.294s

Minor note: in columns mode, a blank cell means that no report was found
in S3, while the word "absent" means that a report was found but the
indicated test was not found in that report.
''',
        formatter_class=HelpFormatter,
    )
    parser.add_argument(
        'mode',
        choices=['columns', 'multiline'],
        help='output format',
    )
    parser.add_argument(
        '--pytorch',
        help='path to local PyTorch clone',
        default='.',
    )
    parser.add_argument(
        '--ref',
        help='starting point (most recent Git ref) to display history for',
        default='master',
    )
    parser.add_argument(
        '--delta',
        type=int,
        help='minimum number of hours between commits',
        default=0,
    )
    parser.add_argument(
        '--sha-length',
        type=int,
        help='length of the prefix of the SHA1 hash to show',
        default=40,
    )
    parser.add_argument(
        '--digits',
        type=int,
        help='(columns) number of digits to display before the decimal point',
        default=4,
    )
    parser.add_argument(
        '--all',
        action='store_true',
        help='(multiline) ignore listed jobs, show all jobs for each commit',
    )
    parser.add_argument(
        '--file',
        help='name of the file containing the test',
    )
    parser.add_argument(
        '--suite',
        help='name of the suite containing the test',
    )
    parser.add_argument(
        'test',
        help='name of the test',
    )
    parser.add_argument(
        'job',
        nargs='*',
        help='names of jobs to display columns for, in order',
        default=[],
    )
    args = parser.parse_args()

    jobs = None if args.all else args.job
    if jobs == []:  # no jobs, and not None (which would mean all jobs)
        parser.error('No jobs specified.')

    commits = get_git_commit_history(path=args.pytorch, ref=args.ref)

    s3 = boto3.resource("s3", config=botocore.config.Config(signature_version=botocore.UNSIGNED))
    bucket = s3.Bucket('ossci-metrics')

    display_history(
        bucket=bucket,
        commits=commits,
        jobs=jobs,
        filename=args.file,
        suite_name=args.suite,
        test_name=args.test,
        delta=args.delta,
        mode=args.mode,
        sha_length=args.sha_length,
        digits=args.digits,
    )


if __name__ == "__main__":
    main()
